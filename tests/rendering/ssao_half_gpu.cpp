#include <Tempest/VulkanApi>
#include <Tempest/Device>
#include <Tempest/Fence>
#include <Tempest/Pixmap>
#include <Tempest/Log>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace Tempest;

static IVec2 nearestPixel(const float* depth, IVec2 size, IVec2 pixel) {
  const IVec2 base((pixel.x/2)*2, (pixel.y/2)*2);
  IVec2 nearest = base;
  for(int y=0; y<2; ++y)
    for(int x=0; x<2; ++x) {
      const IVec2 at(std::min(base.x+x,size.x-1), std::min(base.y+y,size.y-1));
      if(depth[at.y*size.x+at.x]<depth[nearest.y*size.x+nearest.x])
        nearest = at;
      }
  return nearest;
  }

static float reference(const float* guide, IVec2 size, IVec2 pixel, float depth) {
  const float cx = (float(pixel.x)+0.5f)*0.5f-0.5f;
  const float cy = (float(pixel.y)+0.5f)*0.5f-0.5f;
  const float tolerance = std::max(1.f, std::abs(depth)*0.02f);
  double sum = 0, weights = 0;
  for(int y=-1; y<=1; ++y)
    for(int x=-1; x<=1; ++x) {
      const int ax = std::clamp(pixel.x/2+x, 0, size.x-1);
      const int ay = std::clamp(pixel.y/2+y, 0, size.y-1);
      const auto at = (ay*size.x+ax)*2;
      const double dz = (guide[at+1]-depth)/tolerance;
      const double dx = ax-cx, dy = ay-cy;
      const double weight = std::exp2(-0.5*(dx*dx+dy*dy)-dz*dz);
      sum += guide[at]*weight;
      weights += weight;
      }
  return weights>0.0001 ? float(std::clamp(sum/weights,0.0,1.0)) : 1.f;
  }

int main() {
  std::atomic_uint errors{0};
  Log::setOutputCallback([&](Log::Mode mode,const char* text) {
    std::cerr << text << '\n';
    if(mode==Log::Error)
      ++errors;
    });
  try {
    VulkanApi api{ApiFlags::Validation};
    Device device(api);
    auto pipeline = device.pipeline(device.shader("ssao-half.spv"));
    uint64_t samples = 0;
    float worst = 0;
    for(const IVec2 size : {IVec2(1,1),IVec2(1,7),IVec2(7,1),IVec2(7,5),IVec2(17,13),IVec2(1755,810)})
      for(int pattern=0; pattern<6; ++pattern) {
        const IVec2 half((size.x+1)/2,(size.y+1)/2);
        Pixmap depthImage(size.x,size.y,TextureFormat::R32F);
        Pixmap guideImage(half.x,half.y,TextureFormat::RG32F);
        auto depth = static_cast<float*>(depthImage.data());
        auto guide = static_cast<float*>(guideImage.data());
        for(int y=0; y<size.y; ++y)
          for(int x=0; x<size.x; ++x) {
            float z = 1000;
            if(pattern==1 || pattern==2)
              z = (pattern==1 ? x<size.x/2 : x==size.x/2) ? 100.f : 1000.f;
            if(pattern==3)
              z = ((x+y)%2==0) ? 10.f : 1000000.f;
            if(pattern==4)
              z = 1000.f+x*0.1f+y*0.2f;
            depth[y*size.x+x] = z;
            }
        for(int y=0; y<half.y; ++y)
          for(int x=0; x<half.x; ++x) {
            const auto nearest = nearestPixel(depth,size,IVec2(x*2,y*2));
            const float z = depth[nearest.y*size.x+nearest.x];
            const float ao = pattern==0 ? 0.3f : pattern==5 ? 0.f : z<500 ? 0.1f : 0.9f;
            guide[(y*half.x+x)*2] = ao;
            guide[(y*half.x+x)*2+1] = z;
            }
        auto inputDepth = device.texture(depthImage);
        auto inputGuide = device.texture(guideImage);
        std::vector<Vec4> values(size_t(size.x)*size.y);
        auto output = device.ssbo(Uninitialized,values.size()*sizeof(Vec4));
        auto cmd = device.commandBuffer();
        {
        auto enc = cmd.startEncoding(device);
        enc.setBinding(0,inputDepth,Sampler::nearest(ClampMode::ClampToEdge));
        enc.setBinding(1,inputGuide,Sampler::nearest(ClampMode::ClampToEdge));
        enc.setBinding(2,output);
        enc.setPipeline(pipeline);
        enc.dispatchThreads(size.x,size.y);
        }
        auto fence = device.submit(cmd);
        fence.wait();
        device.readBytes(output,values.data(),values.size()*sizeof(Vec4));
        for(int y=0; y<size.y; ++y)
          for(int x=0; x<size.x; ++x) {
            const IVec2 pixel(x,y);
            const auto nearest = nearestPixel(depth,size,pixel);
            const auto& result = values[size_t(y)*size.x+x];
            const float z = depth[y*size.x+x];
            const float expected = reference(guide,half,pixel,z);
            const float error = std::abs(result.x-expected);
            if(!std::isfinite(result.x) || result.x<0 || result.x>1 || error>0.00001f ||
               result.y!=nearest.x || result.z!=nearest.y)
              throw std::runtime_error("Half-resolution SSAO resolve or depth representative differs from reference");
            if((pattern==1 || pattern==2 || pattern==3) &&
               ((z<500 && std::abs(result.x-0.1f)>0.00001f) || (z>=500 && result.x<0.89999f)))
              throw std::runtime_error("SSAO leaked across a depth discontinuity");
            if(pattern==0 && std::abs(result.x-0.3f)>0.00001f)
              throw std::runtime_error("SSAO changed a constant surface");
            if(pattern==5 && result.x!=0)
              throw std::runtime_error("SSAO lost full occlusion");
            worst = std::max(worst,error);
            ++samples;
            }
        }
    device.waitIdle();
    std::cout << samples << " GPU pixels passed; max resolve error " << worst << '\n';
    }
  catch(const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
    }
  return errors==0 ? 0 : 1;
  }
