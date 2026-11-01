#include "fsr2.h"

#include <Tempest/Device>
#include <Tempest/Pixmap>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "sceneglobals.h"
#include "shaders.h"

using namespace Tempest;

// ffxFsr2MaximumBias from ffx_fsr2.cpp
static const float maximumBias[16*16] = {
  2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.876f, 1.809f, 1.772f, 1.753f, 1.748f,
  2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.869f, 1.801f, 1.764f, 1.745f, 1.739f,
  2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.976f, 1.841f, 1.774f, 1.737f, 1.716f, 1.71f,
  2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.914f, 1.784f, 1.716f, 1.673f, 1.649f, 1.641f,
  2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.793f, 1.676f, 1.604f, 1.562f, 1.54f,  1.533f,
  2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.802f, 1.619f, 1.536f, 1.492f, 1.467f, 1.454f, 1.449f,
  2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.812f, 1.575f, 1.496f, 1.456f, 1.432f, 1.416f, 1.408f, 1.405f,
  2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.555f, 1.479f, 1.438f, 1.413f, 1.398f, 1.387f, 1.381f, 1.379f,
  2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.812f, 1.555f, 1.474f, 1.43f,  1.404f, 1.387f, 1.376f, 1.368f, 1.363f, 1.362f,
  2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.802f, 1.575f, 1.479f, 1.43f,  1.401f, 1.382f, 1.369f, 1.36f,  1.354f, 1.351f, 1.35f,
  2.0f,   2.0f,   1.976f, 1.914f, 1.793f, 1.619f, 1.496f, 1.438f, 1.404f, 1.382f, 1.367f, 1.357f, 1.349f, 1.344f, 1.341f, 1.34f,
  1.876f, 1.869f, 1.841f, 1.784f, 1.676f, 1.536f, 1.456f, 1.413f, 1.387f, 1.369f, 1.357f, 1.347f, 1.341f, 1.336f, 1.333f, 1.332f,
  1.809f, 1.801f, 1.774f, 1.716f, 1.604f, 1.492f, 1.432f, 1.398f, 1.376f, 1.36f,  1.349f, 1.341f, 1.335f, 1.33f,  1.328f, 1.327f,
  1.772f, 1.764f, 1.737f, 1.673f, 1.562f, 1.467f, 1.416f, 1.387f, 1.368f, 1.354f, 1.344f, 1.336f, 1.33f,  1.326f, 1.323f, 1.323f,
  1.753f, 1.745f, 1.716f, 1.649f, 1.54f,  1.454f, 1.408f, 1.381f, 1.363f, 1.351f, 1.341f, 1.333f, 1.328f, 1.323f, 1.321f, 1.32f,
  1.748f, 1.739f, 1.71f,  1.641f, 1.533f, 1.449f, 1.405f, 1.379f, 1.362f, 1.35f,  1.34f,  1.332f, 1.327f, 1.323f, 1.32f,  1.319f,
  };

static constexpr int32_t ShadingChangeMip = 4; // FFX_FSR2_SHADING_CHANGE_MIP_LEVEL
static constexpr float   Sharpness        = 0.5f;

uint32_t Fsr2::jitterPhases(int renderW, int displayW) {
  const float ratio = float(displayW)/float(std::max(renderW,1));
  return uint32_t(8.f*ratio*ratio);
  }

void Fsr2::resize(Size rs, Size ds) {
  auto& device = Resources::device();
  for(auto* i:{&motion, &preparedColor, &prevNearestDepth, &dilatedMotion[0], &dilatedMotion[1], &dilatedDepth,
               &lockInputLuma, &dilatedReactive, &luminance, &spdAtomic, &autoExposure, &lockStatus[0], &lockStatus[1],
               &newLocks, &upscaled[0], &upscaled[1], &lumaHistory[0], &lumaHistory[1], &output})
    Resources::recycle(std::move(*i));

  renderSize  = rs;
  displaySize = ds;

  // R16F/RG16F of the reference become 32 bit floats, Tempest has no 16 bit float formats besides RGBA16F
  motion           = device.image2d(TextureFormat::RG32F,   rs);
  preparedColor    = device.image2d(TextureFormat::RGBA16F, rs);
  prevNearestDepth = device.image2d(TextureFormat::R32U,    rs);
  dilatedMotion[0] = device.image2d(TextureFormat::RG32F,   rs);
  dilatedMotion[1] = device.image2d(TextureFormat::RG32F,   rs);
  dilatedDepth     = device.image2d(TextureFormat::R32F,    rs);
  lockInputLuma    = device.image2d(TextureFormat::R32F,    rs);
  dilatedReactive  = device.image2d(TextureFormat::RG8,     rs);
  luminance        = device.image2d(TextureFormat::R32F,    uint32_t(std::max(rs.w/2,1)), uint32_t(std::max(rs.h/2,1)), true);
  spdAtomic        = device.image2d(TextureFormat::R32U,    1, 1);
  autoExposure     = device.image2d(TextureFormat::RG32F,   1, 1);

  lockStatus[0]    = device.image2d(TextureFormat::RG32F,   ds);
  lockStatus[1]    = device.image2d(TextureFormat::RG32F,   ds);
  newLocks         = device.image2d(TextureFormat::R8,      ds);
  upscaled[0]      = device.image2d(TextureFormat::RGBA16F, ds);
  upscaled[1]      = device.image2d(TextureFormat::RGBA16F, ds);
  lumaHistory[0]   = device.image2d(TextureFormat::RGBA8,   ds);
  lumaHistory[1]   = device.image2d(TextureFormat::RGBA8,   ds);
  output           = device.image2d(TextureFormat::RGBA16F, ds);

  if(maxBiasLut.isEmpty()) {
    // the reference stores it as snorm of bias/2, the shader doubles it again
    Pixmap bias(16, 16, TextureFormat::R32F);
    auto*  px = reinterpret_cast<float*>(bias.data());
    for(size_t i=0; i<16*16; ++i)
      px[i] = maximumBias[i]*0.5f;
    maxBiasLut = device.texture(bias, false);

    Pixmap zero(1, 1, TextureFormat::R8);
    std::memset(zero.data(), 0, 1);
    noReactivity = device.texture(zero, false);
    }
  firstUse = true;
  }

void Fsr2::clear(Encoder<CommandBuffer>& cmd, const StorageImage& img, const Vec4& v, uint32_t mip) {
  cmd.setBinding(0, img, Sampler::nearest(), mip);
  cmd.setPushData(&v, sizeof(v));
  cmd.setPipeline(Shaders::inst().fsr2.clear);
  cmd.dispatchThreads(size_t(std::max(img.w()>>mip, 1)), size_t(std::max(img.h()>>mip, 1)));
  }

void Fsr2::clearU(Encoder<CommandBuffer>& cmd, const StorageImage& img) {
  const Vec4 v = {};
  cmd.setBinding(0, img);
  cmd.setPushData(&v, sizeof(v));
  cmd.setPipeline(Shaders::inst().fsr2.clearU);
  cmd.dispatchThreads(img.size());
  }

const StorageImage& Fsr2::draw(Encoder<CommandBuffer>& cmd, uint8_t fId, const SceneGlobals& scene, const Inputs& in) {
  auto&      device = Resources::device();
  auto&      sh     = Shaders::inst().fsr2;
  const Size rs     = in.color->size();

  bool reset = in.reset;
  if(rs!=renderSize || in.display!=displaySize) {
    resize(rs, in.display);
    reset = true;
    }

  // ping-pong of the reference: which half is read this frame and which is written
  const bool odd     = (frame & 1)!=0;
  const int  readId  = odd ? 1 : 0;
  const int  writeId = odd ? 0 : 1;
  frame = (frame+1)%16;

  cmd.setDebugMarker("FSR2");
  cmd.setFramebuffer({});

  if(firstUse) {
    firstUse = false;
    for(auto* i:{&lockStatus[0], &lockStatus[1], &upscaled[0], &upscaled[1], &lumaHistory[0], &lumaHistory[1],
                 &dilatedMotion[0], &dilatedMotion[1], &preparedColor, &newLocks})
      clear(cmd, *i, Vec4());
    clearU(cmd, spdAtomic);
    clearU(cmd, prevNearestDepth);
    }
  if(reset) {
    clear(cmd, lockStatus[readId], Vec4());
    clear(cmd, upscaled[readId],   Vec4());
    for(uint32_t mip=0; mip<luminance.mipCount(); ++mip)
      clear(cmd, luminance, Vec4(), mip);
    clear(cmd, autoExposure, Vec4(-1.f, 1e8f, 0, 0));
    }

  // constants, as fsr2Dispatch in ffx_fsr2.cpp sets them
  {
    const float aspect = float(rs.w)/float(rs.h);
    const float fMin   = std::min(in.zNear, in.zFar);
    const float fMax   = std::max(in.zNear, in.zFar);
    const float fQ     = fMax/(fMin - fMax);
    const uint32_t phases = jitterPhases(rs.w, in.display.w);

    cst.renderSize[0]                   = rs.w;
    cst.renderSize[1]                   = rs.h;
    cst.maxRenderSize[0]                = rs.w;
    cst.maxRenderSize[1]                = rs.h;
    cst.displaySize[0]                  = in.display.w;
    cst.displaySize[1]                  = in.display.h;
    cst.inputColorResourceDimensions[0] = rs.w;
    cst.inputColorResourceDimensions[1] = rs.h;
    cst.lumaMipLevelToUse               = ShadingChangeMip;
    cst.lumaMipDimensions[0]            = int32_t(float(rs.w)/float(2 << ShadingChangeMip));
    cst.lumaMipDimensions[1]            = int32_t(float(rs.h)/float(2 << ShadingChangeMip));
    cst.frameIndex                      = reset ? 0 : cst.frameIndex+1;

    cst.deviceToViewDepth[0]            = -fQ;
    cst.deviceToViewDepth[1]            = fQ*fMin;
    cst.deviceToViewDepth[2]            = aspect/in.cotHalfFovY;
    cst.deviceToViewDepth[3]            = 1.f/in.cotHalfFovY;
    cst.jitterOffset[0]                 = in.jitter.x;
    cst.jitterOffset[1]                 = in.jitter.y;
    // fsr2_motion.comp writes UV offsets already
    cst.motionVectorScale[0]            = 1.f;
    cst.motionVectorScale[1]            = 1.f;
    cst.downscaleFactor[0]              = float(rs.w)/float(in.display.w);
    cst.downscaleFactor[1]              = float(rs.h)/float(in.display.h);
    cst.motionVectorJitterCancellation[0] = 0;
    cst.motionVectorJitterCancellation[1] = 0;
    cst.previousFramePreExposure        = reset ? 1.f : cst.preExposure;
    cst.preExposure                     = 1.f;
    cst.tanHalfFOV                      = aspect/in.cotHalfFovY;
    if(reset || cst.jitterPhaseCount==0)
      cst.jitterPhaseCount = float(phases);
    else if(float(phases)>cst.jitterPhaseCount)
      cst.jitterPhaseCount += 1;
    else if(float(phases)<cst.jitterPhaseCount)
      cst.jitterPhaseCount -= 1;
    cst.deltaTime                       = std::clamp(in.frameTimeMs/1000.f, 0.f, 1.f);
    cst.dynamicResChangeFactor          = 0;
    cst.viewSpaceToMetersFactor         = 0.01f; // centimeters
  }

  SpdConstants spd = {};
  const uint32_t spdX = uint32_t(rs.w-1)/64 + 1;
  const uint32_t spdY = uint32_t(rs.h-1)/64 + 1;
  spd.numWorkGroups = spdX*spdY;
  spd.mips          = uint32_t(std::min(std::floor(std::log2(float(std::max(rs.w, rs.h)))), 12.f));
  spd.renderSize[0] = uint32_t(rs.w);
  spd.renderSize[1] = uint32_t(rs.h);

  RcasConstants rcas = {};
  const float sharp = std::exp2(-(2.f - 2.f*Sharpness));
  std::memcpy(&rcas.config[0], &sharp, sizeof(sharp));

  if(cbFsr2[fId].isEmpty()) {
    cbFsr2[fId] = device.ubo(cst);
    cbSpd [fId] = device.ubo(spd);
    cbRcas[fId] = device.ubo(rcas);
    } else {
    cbFsr2[fId].update(&cst);
    cbSpd [fId].update(&spd);
    cbRcas[fId].update(&rcas);
    }

  const auto point  = Sampler::nearest (ClampMode::ClampToEdge);
  const auto linear = Sampler::bilinear(ClampMode::ClampToEdge);
  const Size srcGroups((rs.w+7)/8, (rs.h+7)/8);
  const Size dstGroups((in.display.w+7)/8, (in.display.h+7)/8);

  // our motion vectors as UV offsets, the sky and water by camera reprojection
  cmd.setBinding(0, scene.uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, motion);
  cmd.setBinding(2, *in.velocity, point);
  cmd.setBinding(3, *in.depth,    point);
  cmd.setPipeline(sh.motion);
  cmd.dispatchThreads(rs);

  cmd.setBinding(30, point);
  cmd.setBinding(31, linear);

  cmd.setDebugMarker("FSR2-luminance");
  cmd.setBinding(0, *in.color, point);
  cmd.setBinding(1, spdAtomic);
  cmd.setBinding(2, luminance, point, uint32_t(ShadingChangeMip));
  cmd.setBinding(3, luminance, point, 5);
  cmd.setBinding(4, autoExposure);
  cmd.setBinding(5, cbFsr2[fId]);
  cmd.setBinding(6, cbSpd[fId]);
  cmd.setPipeline(sh.luminance);
  cmd.dispatch(spdX, spdY);

  cmd.setDebugMarker("FSR2-reconstruct");
  cmd.setBinding(0,  motion, point);
  cmd.setBinding(1,  *in.depth, point);
  cmd.setBinding(2,  *in.color, point);
  cmd.setBinding(3,  autoExposure, point);
  cmd.setBinding(4,  lumaHistory[readId], point);
  cmd.setBinding(5,  prevNearestDepth);
  cmd.setBinding(6,  dilatedMotion[readId]);
  cmd.setBinding(7,  dilatedDepth);
  cmd.setBinding(8,  preparedColor);
  cmd.setBinding(9,  lumaHistory[writeId]);
  cmd.setBinding(11, lockInputLuma);
  cmd.setBinding(12, cbFsr2[fId]);
  cmd.setPipeline(sh.prevDepth);
  cmd.dispatch(size_t(srcGroups.w), size_t(srcGroups.h));

  cmd.setDebugMarker("FSR2-depth-clip");
  cmd.setBinding(0,  prevNearestDepth, point);
  cmd.setBinding(1,  dilatedMotion[readId], point);
  cmd.setBinding(2,  dilatedDepth, point);
  cmd.setBinding(3,  noReactivity, point);
  cmd.setBinding(4,  noReactivity, point);
  cmd.setBinding(6,  dilatedMotion[writeId], point);
  cmd.setBinding(7,  motion, point);
  cmd.setBinding(8,  *in.color, point);
  cmd.setBinding(9,  *in.depth, point);
  cmd.setBinding(10, autoExposure, point);
  cmd.setBinding(12, dilatedReactive);
  cmd.setBinding(13, preparedColor);
  cmd.setBinding(14, cbFsr2[fId]);
  cmd.setPipeline(sh.depthClip);
  cmd.dispatch(size_t(srcGroups.w), size_t(srcGroups.h));

  cmd.setDebugMarker("FSR2-lock");
  cmd.setBinding(0, lockInputLuma, point);
  cmd.setBinding(1, newLocks);
  cmd.setBinding(2, prevNearestDepth);
  cmd.setBinding(3, cbFsr2[fId]);
  cmd.setPipeline(sh.lock);
  cmd.dispatch(size_t(srcGroups.w), size_t(srcGroups.h));

  cmd.setDebugMarker("FSR2-accumulate");
  cmd.setBinding(0,  autoExposure, point);
  cmd.setBinding(1,  dilatedReactive, point);
  cmd.setBinding(2,  dilatedMotion[readId], point);
  cmd.setBinding(3,  upscaled[readId], point);
  cmd.setBinding(4,  lockStatus[readId], point);
  cmd.setBinding(6,  preparedColor, point);
  cmd.setBinding(8,  maxBiasLut, point); // Lanczos is computed, the LUT binding only has to exist
  cmd.setBinding(9,  maxBiasLut, linear);
  cmd.setBinding(10, luminance, point);
  cmd.setBinding(11, autoExposure, point);
  cmd.setBinding(12, lumaHistory[readId], point);
  cmd.setBinding(13, upscaled[writeId]);
  cmd.setBinding(14, lockStatus[writeId]);
  cmd.setBinding(15, output);
  cmd.setBinding(16, newLocks);
  cmd.setBinding(17, lumaHistory[writeId]);
  cmd.setBinding(18, cbFsr2[fId]);
  cmd.setPipeline(sh.accumulate);
  cmd.dispatch(size_t(dstGroups.w), size_t(dstGroups.h));

  cmd.setDebugMarker("FSR2-sharpen");
  cmd.setBinding(0, autoExposure, point);
  cmd.setBinding(1, upscaled[writeId], point);
  cmd.setBinding(2, output);
  cmd.setBinding(3, cbFsr2[fId]);
  cmd.setBinding(4, cbRcas[fId]);
  cmd.setPipeline(sh.rcas);
  cmd.dispatch(size_t((in.display.w+15)/16), size_t((in.display.h+15)/16));

  return output;
  }
