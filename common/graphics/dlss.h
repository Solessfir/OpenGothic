#pragma once

#include <Tempest/CommandBuffer>
#include <Tempest/Encoder>
#include <Tempest/StorageImage>

class SceneGlobals;

// NVIDIA DLSS super resolution through NGX. Only built with OPENGOTHIC_DLSS_SDK, otherwise never supported.
class Dlss final {
  public:
    struct Inputs {
      const Tempest::Attachment* color    = nullptr; // HDR, render resolution, jittered
      const Tempest::ZBuffer*    depth    = nullptr;
      const Tempest::Attachment* velocity = nullptr; // gbuffer motion vectors, see encodeVelocity
      Tempest::Size              display;
      Tempest::Vec2              jitter;             // content shift of this frame, in render pixels
      float                      frameTimeMs = 16;
      bool                       reset       = false;
      };

    Dlss() = default;
    ~Dlss();

    // NGX needs its Vulkan extensions enabled at instance and device creation
    static void requestExtensions();
    // Initializes NGX on first use
    static bool isSupported();

    // HDR output at display resolution, nullptr when DLSS could not run
    const Tempest::StorageImage* draw(Tempest::Encoder<Tempest::CommandBuffer>& cmd, const SceneGlobals& scene, const Inputs& in);

  private:
    void release();

    void*                 feature = nullptr;
    Tempest::Size         renderSize, displaySize;
    Tempest::StorageImage motion, depth, output;
  };
