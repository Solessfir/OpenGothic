#include "dlss.h"

#if defined(OPENGOTHIC_WITH_DLSS)

#include <Tempest/Device>
#include <Tempest/Log>
#include <Tempest/VulkanApi>

#include <vulkan/vulkan.h>
#include <nvsdk_ngx_vk.h>
#include <nvsdk_ngx_helpers.h>
#include <nvsdk_ngx_helpers_vk.h>

#include "graphics/sceneglobals.h"
#include "graphics/shaders.h"
#include "resources.h"

using namespace Tempest;

namespace {
enum class State : uint8_t { Unknown, Supported, Unsupported };
State                 state  = State::Unknown;
NVSDK_NGX_Parameter*  params = nullptr;
VkDevice              device = VK_NULL_HANDLE;

// any stable id works for a custom engine outside NVIDIA's application registry
const char* ProjectId = "5b0f8a3e-2c71-4d9e-a6f4-9e1d3c7b2a58";

NVSDK_NGX_Resource_VK resource(const AbstractGraphicsApi::NativeImage& img, Size sz, bool rw) {
  VkImageSubresourceRange range = {};
  range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  range.levelCount = 1;
  range.layerCount = 1;
  return NVSDK_NGX_Create_ImageView_Resource_VK(VkImageView(img.view), VkImage(img.image), range, VkFormat(img.format),
                                                uint32_t(sz.w), uint32_t(sz.h), rw);
  }

// DLSS runs at a ratio inside the range of one of its quality modes
NVSDK_NGX_PerfQuality_Value qualityFor(Size render, Size display) {
  if(render==display)
    return NVSDK_NGX_PerfQuality_Value_DLAA;
  const NVSDK_NGX_PerfQuality_Value modes[] = {
    NVSDK_NGX_PerfQuality_Value_MaxQuality, NVSDK_NGX_PerfQuality_Value_Balanced,
    NVSDK_NGX_PerfQuality_Value_MaxPerf,    NVSDK_NGX_PerfQuality_Value_UltraPerformance,
    };
  for(auto m:modes) {
    unsigned optW=0, optH=0, maxW=0, maxH=0, minW=0, minH=0;
    float    sharp = 0;
    if(NVSDK_NGX_FAILED(NGX_DLSS_GET_OPTIMAL_SETTINGS(params, uint32_t(display.w), uint32_t(display.h), m,
                                                      &optW, &optH, &maxW, &maxH, &minW, &minH, &sharp)))
      continue;
    if(minW<=uint32_t(render.w) && uint32_t(render.w)<=maxW && minH<=uint32_t(render.h) && uint32_t(render.h)<=maxH)
      return m;
    }
  return NVSDK_NGX_PerfQuality_Value_MaxQuality;
  }
}

void Dlss::requestExtensions() {
  unsigned     instCount = 0, devCount = 0;
  const char** instExt   = nullptr;
  const char** devExt    = nullptr;
  if(NVSDK_NGX_FAILED(NVSDK_NGX_VULKAN_RequiredExtensions(&instCount, &instExt, &devCount, &devExt)))
    return;
  std::vector<std::string> inst(instExt, instExt+instCount), dev(devExt, devExt+devCount);
  VulkanApi::requestExtensions(std::move(inst), std::move(dev));
  }

bool Dlss::isSupported() {
  if(state!=State::Unknown)
    return state==State::Supported;
  state = State::Unsupported;

  auto& dev = Resources::device();
  const auto nat = dev.native();
  if(nat.device==nullptr)
    return false;

  device = VkDevice(nat.device);
  auto r = NVSDK_NGX_VULKAN_Init_with_ProjectID(ProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, "1.0", L".",
                                                VkInstance(nat.instance), VkPhysicalDevice(nat.physicalDevice), device,
                                                PFN_vkGetInstanceProcAddr(nat.getInstanceProcAddr),
                                                PFN_vkGetDeviceProcAddr(nat.getDeviceProcAddr));
  if(NVSDK_NGX_FAILED(r)) {
    Log::i("DLSS: NGX init failed, code ", int(r));
    return false;
    }
  if(NVSDK_NGX_FAILED(NVSDK_NGX_VULKAN_GetCapabilityParameters(&params))) {
    Log::i("DLSS: no NGX capability parameters");
    NVSDK_NGX_VULKAN_Shutdown1(device);
    return false;
    }
  int available = 0, needsDriver = 0;
  params->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &available);
  params->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsDriver);
  if(available==0) {
    Log::i("DLSS: not available", needsDriver ? ", needs a newer driver" : "");
    NVSDK_NGX_VULKAN_DestroyParameters(params);
    NVSDK_NGX_VULKAN_Shutdown1(device);
    params = nullptr;
    return false;
    }
  Log::i("DLSS: available");
  state = State::Supported;
  return true;
  }

Dlss::~Dlss() {
  if(feature!=nullptr) {
    Resources::device().waitIdle();
    release();
    }
  }

void Dlss::release() {
  if(feature==nullptr)
    return;
  NVSDK_NGX_VULKAN_ReleaseFeature(reinterpret_cast<NVSDK_NGX_Handle*>(feature));
  feature = nullptr;
  }

const StorageImage* Dlss::draw(Encoder<CommandBuffer>& cmd, const SceneGlobals& scene, const Inputs& in) {
  if(!isSupported())
    return nullptr;

  auto&      dev = Resources::device();
  auto&      sh  = Shaders::inst().dlss;
  const Size rs  = in.color->size();

  bool reset = in.reset;
  if(rs!=renderSize || in.display!=displaySize) {
    if(feature!=nullptr) {
      dev.waitIdle();
      release();
      }
    Resources::recycle(std::move(motion));
    Resources::recycle(std::move(depth));
    Resources::recycle(std::move(output));
    motion      = dev.image2d(TextureFormat::RG32F,   rs);
    depth       = dev.image2d(TextureFormat::R32F,    rs);
    output      = dev.image2d(TextureFormat::RGBA16F, in.display);
    renderSize  = rs;
    displaySize = in.display;
    reset       = true;
    }

  cmd.setDebugMarker("DLSS");
  cmd.setFramebuffer({});
  cmd.setBinding(0, scene.uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, motion);
  cmd.setBinding(2, *in.velocity, Sampler::nearest());
  cmd.setBinding(3, *in.depth,    Sampler::nearest());
  cmd.setBinding(4, depth);
  cmd.setPipeline(sh.inputs);
  cmd.dispatchThreads(rs);

  const uint32_t never = 0;
  cmd.setBinding(0, *in.color, Sampler::nearest());
  cmd.setBinding(1, depth,     Sampler::nearest());
  cmd.setBinding(2, motion,    Sampler::nearest());
  cmd.setBinding(3, output);
  cmd.setPushData(&never, sizeof(never));
  cmd.setPipeline(sh.sync);
  cmd.dispatch(1);

  auto colorRes  = resource(cmd.native(*in.color), rs, false);
  auto depthRes  = resource(cmd.native(depth),     rs, false);
  auto motionRes = resource(cmd.native(motion),    rs, false);
  auto outRes    = resource(cmd.native(output),    in.display, true);

  auto vk = VkCommandBuffer(cmd.nativeBegin());
  bool ok = true;
  if(feature==nullptr) {
    NVSDK_NGX_DLSS_Create_Params create = {};
    create.Feature.InWidth            = uint32_t(rs.w);
    create.Feature.InHeight           = uint32_t(rs.h);
    create.Feature.InTargetWidth      = uint32_t(in.display.w);
    create.Feature.InTargetHeight     = uint32_t(in.display.h);
    create.Feature.InPerfQualityValue = qualityFor(rs, in.display);
    create.InFeatureCreateFlags       = NVSDK_NGX_DLSS_Feature_Flags_IsHDR | NVSDK_NGX_DLSS_Feature_Flags_MVLowRes |
                                        NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;
    NVSDK_NGX_Handle* h = nullptr;
    auto r = NGX_VULKAN_CREATE_DLSS_EXT(vk, 1, 1, &h, params, &create);
    if(NVSDK_NGX_FAILED(r)) {
      Log::e("DLSS: feature creation failed, code ", int(r));
      ok = false;
      } else {
      feature = h;
      }
    }

  if(ok) {
    NVSDK_NGX_VK_DLSS_Eval_Params eval = {};
    eval.Feature.pInColor              = &colorRes;
    eval.Feature.pInOutput             = &outRes;
    eval.pInDepth                      = &depthRes;
    eval.pInMotionVectors              = &motionRes;
    // DLSS takes the sample offset, the opposite of how far the content moved (the sharpest of the four sign choices)
    eval.InJitterOffsetX               = -in.jitter.x;
    eval.InJitterOffsetY               = -in.jitter.y;
    eval.InRenderSubrectDimensions     = {uint32_t(rs.w), uint32_t(rs.h)};
    eval.InReset                       = reset ? 1 : 0;
    // motion is a UV offset, DLSS wants render pixels
    eval.InMVScaleX                    = float(rs.w);
    eval.InMVScaleY                    = float(rs.h);
    eval.InFrameTimeDeltaInMsec        = in.frameTimeMs;
    auto r = NGX_VULKAN_EVALUATE_DLSS_EXT(vk, reinterpret_cast<NVSDK_NGX_Handle*>(feature), params, &eval);
    if(NVSDK_NGX_FAILED(r)) {
      Log::e("DLSS: evaluation failed, code ", int(r));
      ok = false;
      }
    }
  cmd.nativeEnd();

  if(!ok) {
    // keep the frame alive on FSR2 or the plain upscale from now on
    state = State::Unsupported;
    return nullptr;
    }
  return &output;
  }

#else

void Dlss::requestExtensions() {}
bool Dlss::isSupported() { return false; }
Dlss::~Dlss() {}
void Dlss::release() { feature = nullptr; }

const Tempest::StorageImage* Dlss::draw(Tempest::Encoder<Tempest::CommandBuffer>&, const SceneGlobals&, const Inputs&) {
  return nullptr;
  }

#endif
