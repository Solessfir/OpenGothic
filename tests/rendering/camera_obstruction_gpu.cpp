#include <Tempest/VulkanApi>
#include <Tempest/Device>
#include <Tempest/CommandBuffer>
#include <Tempest/Fence>
#include <Tempest/Log>
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

int main() {
  using namespace Tempest;
  std::atomic_uint errors{0};
  Log::setOutputCallback([&](Log::Mode mode,const char* text) {
    if(mode==Log::Error) { ++errors; std::cerr << text << '\n'; }
    });
  try {
    VulkanApi api{ApiFlags::Validation};
    Device device(api);
    auto pipeline=device.pipeline(device.shader("camera-obstruction.spv"));
    std::array<std::array<float,4>,201> values{};
    auto output=device.ssbo(Uninitialized,sizeof(values));
    auto cmd=device.commandBuffer();
    {
      auto enc=cmd.startEncoding(device);
      enc.setBinding(0,output);
      enc.setPipeline(pipeline);
      enc.dispatchThreads(uint32_t(values.size()),1);
    }
    auto fence=device.submit(cmd);
    fence.wait();
    device.readBytes(output,values.data(),sizeof(values));
    std::array<float,16> thresholds{};
    for(size_t i=0;i<values.size();++i) {
      const double t=std::clamp((double(i*i)-45*45)/(180*180-45*45),0.0,1.0);
      const double expected=t*t*(3-2*t);
      if(!std::isfinite(values[i][0]) || std::abs(values[i][0]-expected)>0.00001)
        throw std::runtime_error("Camera fade differs from the distance curve");
      if(i>0 && values[i][0]<values[i-1][0])
        throw std::runtime_error("Camera fade is not monotonic");
      if(i<16) thresholds[i]=values[i][1];
      if(i>=16 && values[i][1]!=thresholds[i%16])
        throw std::runtime_error("Dither pattern is not periodic");
      const double edge=(105.0*105.0-90.0*90.0)/(120.0*120.0-90.0*90.0);
      const std::array<double,12> corridor={0,0,0,0,1,edge*edge*(3-2*edge),0.5,1,1,1,1,1};
      for(size_t c=2;c<4;++c)
        if(!std::isfinite(values[i][c]) || std::abs(values[i][c]-corridor[i%12])>0.00002)
          throw std::runtime_error("Camera corridor differs from expected coverage or fades behind the player");
    }
    std::sort(thresholds.begin(),thresholds.end());
    for(size_t i=0;i<thresholds.size();++i)
      if(thresholds[i]!=(float(i)+0.5f)/16.f)
        throw std::runtime_error("Dither coverage is biased");
    device.waitIdle();
  } catch(const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  return errors==0 ? 0 : 1;
  }
