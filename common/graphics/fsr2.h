#pragma once

#include <Tempest/CommandBuffer>
#include <Tempest/Encoder>
#include <Tempest/StorageImage>
#include <Tempest/Texture2d>
#include <Tempest/UniformBuffer>

#include "resources.h"

class SceneGlobals;

// AMD FSR 2.2.1 temporal upscaler, driven with the reference passes from shader/antialiasing/fsr2
class Fsr2 final {
  public:
    struct Inputs {
      const Tempest::Attachment* color    = nullptr; // HDR, render resolution, jittered
      const Tempest::ZBuffer*    depth    = nullptr;
      const Tempest::Attachment* velocity = nullptr; // gbuffer motion vectors, see encodeVelocity
      Tempest::Size              display;
      Tempest::Vec2              jitter;             // content shift of this frame, in render pixels
      float                      zNear       = 0;
      float                      zFar        = 0;
      float                      cotHalfFovY = 1;
      float                      frameTimeMs = 16;
      bool                       reset       = false;
      };

    // HDR output at display resolution
    const Tempest::StorageImage& draw(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId,
                                      const SceneGlobals& scene, const Inputs& in);

    static uint32_t jitterPhases(int renderW, int displayW);

  private:
    struct Constants {
      int32_t  renderSize[2];
      int32_t  maxRenderSize[2];
      int32_t  displaySize[2];
      int32_t  inputColorResourceDimensions[2];
      int32_t  lumaMipDimensions[2];
      int32_t  lumaMipLevelToUse;
      int32_t  frameIndex;

      float    deviceToViewDepth[4];
      float    jitterOffset[2];
      float    motionVectorScale[2];
      float    downscaleFactor[2];
      float    motionVectorJitterCancellation[2];
      float    preExposure;
      float    previousFramePreExposure;
      float    tanHalfFOV;
      float    jitterPhaseCount;
      float    deltaTime;
      float    dynamicResChangeFactor;
      float    viewSpaceToMetersFactor;
      float    padding;
      };

    struct SpdConstants {
      uint32_t mips;
      uint32_t numWorkGroups;
      uint32_t workGroupOffset[2];
      uint32_t renderSize[2];
      uint32_t padding[2];
      };

    struct RcasConstants {
      uint32_t config[4];
      };

    void resize(Tempest::Size render, Tempest::Size display);
    void clear(Tempest::Encoder<Tempest::CommandBuffer>& cmd, const Tempest::StorageImage& img, const Tempest::Vec4& v, uint32_t mip = 0);
    void clearU(Tempest::Encoder<Tempest::CommandBuffer>& cmd, const Tempest::StorageImage& img);

    Tempest::Size                  renderSize, displaySize;
    bool                           firstUse = true;
    uint32_t                       frame    = 0;
    Constants                      cst      = {};

    Tempest::StorageImage          motion, preparedColor, prevNearestDepth, dilatedMotion[2], dilatedDepth;
    Tempest::StorageImage          lockInputLuma, dilatedReactive, luminance, spdAtomic, autoExposure;
    Tempest::StorageImage          lockStatus[2], newLocks, upscaled[2], lumaHistory[2], output;
    Tempest::Texture2d             maxBiasLut, noReactivity;

    Tempest::UniformBuffer<Constants>     cbFsr2[Resources::MaxFramesInFlight];
    Tempest::UniformBuffer<SpdConstants>  cbSpd [Resources::MaxFramesInFlight];
    Tempest::UniformBuffer<RcasConstants> cbRcas[Resources::MaxFramesInFlight];
  };
