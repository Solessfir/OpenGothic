#include <Tempest/VulkanApi>
#include <Tempest/Device>
#include <Tempest/Fence>
#include <Tempest/Pixmap>
#include <Tempest/Log>

#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace Tempest;

// The generated HDR inputs are integers in [0,31], exactly representable in all channels.
static uint32_t packHdr(float v) {
  if(v==0)
    return 0;
  const uint32_t bits = std::bit_cast<uint32_t>(v);
  const uint32_t exponent = ((bits>>23)&255)-127+15;
  const uint32_t rg = (exponent<<6)|((bits>>17)&63);
  const uint32_t b = (exponent<<5)|((bits>>18)&31);
  return rg | (rg<<11) | (b<<22);
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
    auto reference = device.pipeline(device.shader("reference.spv"));
    auto paired = device.pipeline(device.shader("paired.spv"));
    double worst = 0, total = 0;
    size_t samples = 0;
    for(const TextureFormat format : {TextureFormat::RGBA8,TextureFormat::R11G11B10UF})
    for(const IVec2 inputSize : {IVec2(7,5),IVec2(1170,540),IVec2(1755,810)})
    for(int pattern=0; pattern<4; ++pattern) {
      const bool hdr = format==TextureFormat::R11G11B10UF;
      const double range = hdr ? 31.0 : 1.0;
      Pixmap image(inputSize.x,inputSize.y,format);
      auto data = static_cast<uint8_t*>(image.data());
      for(int i=0; i<inputSize.x*inputSize.y; ++i) {
        const uint8_t v = pattern==0 ? 255 : pattern==1 ? (i==inputSize.x*inputSize.y/2 ? 255 : 0) :
                          pattern==2 ? (i%2==0 ? 0 : 255) : uint8_t((i*97+31)%256);
        if(hdr) {
          static_cast<uint32_t*>(image.data())[i] = packHdr(float(v/8));
          continue;
          }
        data[4*i] = data[4*i+1] = data[4*i+2] = v;
        data[4*i+3] = 255;
        }
      auto input = device.texture(image);
      const std::vector<IVec2> sizes = inputSize.x==7 ?
          std::vector<IVec2>{{14,10},{9,6},{31,19},{7,5}} : std::vector<IVec2>{{2340,1080}};
      for(const IVec2 size : sizes) {
        std::vector<Vec4> a(size_t(size.x*size.y)), b(a.size());
        auto outputA = device.ssbo(Uninitialized,a.size()*sizeof(Vec4));
        auto outputB = device.ssbo(Uninitialized,b.size()*sizeof(Vec4));
        auto cmd = device.commandBuffer();
        {
        auto enc = cmd.startEncoding(device);
        enc.setPushData(size);
        enc.setBinding(0,input,Sampler::nearest(ClampMode::ClampToEdge));
        enc.setBinding(1,outputA);
        enc.setPipeline(reference);
        enc.dispatchThreads(size.x,size.y);
        enc.setBinding(0,input,Sampler::bilinear(ClampMode::ClampToEdge));
        enc.setBinding(1,outputB);
        enc.setPipeline(paired);
        enc.dispatchThreads(size.x,size.y);
        }
        auto fence = device.submit(cmd);
        fence.wait();
        device.readBytes(outputA,a.data(),a.size()*sizeof(Vec4));
        device.readBytes(outputB,b.data(),b.size()*sizeof(Vec4));
        for(size_t i=0; i<a.size(); ++i) {
          const double difference = std::max({std::abs(double(a[i].x)-double(b[i].x)),
                                             std::abs(double(a[i].y)-double(b[i].y)),
                                             std::abs(double(a[i].z)-double(b[i].z))}) / range;
          const bool finite = std::isfinite(a[i].x) && std::isfinite(a[i].y) && std::isfinite(a[i].z) &&
                              std::isfinite(b[i].x) && std::isfinite(b[i].y) && std::isfinite(b[i].z);
          if(!finite || difference>0.01) {
            std::cerr << "Pattern " << pattern << ", HDR " << hdr << ", input " << inputSize.x << 'x' << inputSize.y
                      << ", size " << size.x << 'x' << size.y
                      << ", pixel " << i << ": reference " << a[i].x << ", paired " << b[i].x
                      << ", difference " << difference << '\n';
            throw std::runtime_error("GPU filters differ by more than 0.01 normalized intensity");
            }
          worst = std::max(worst,difference);
          total += difference;
          ++samples;
          }
        }
      }
    device.waitIdle();
    std::cout << samples << " GPU samples; max error " << worst << ", mean error " << total/samples << '\n';
    }
  catch(const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
    }
  // Device destruction also emits validation errors, so check after it completes.
  if(errors!=0) {
    std::cerr << "Vulkan validation or engine errors occurred\n";
    return 1;
    }
  return 0;
  }
