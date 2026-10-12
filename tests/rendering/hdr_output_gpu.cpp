#include <Tempest/VulkanApi>
#include <Tempest/Device>
#include <Tempest/Fence>
#include <Tempest/Log>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace Tempest;

static double pq(double nits) {
  const double p = std::pow(std::clamp(nits/10000.0,0.0,1.0),2610.0/16384.0);
  return std::pow((3424.0/4096.0+(2413.0/128.0)*p)/(1.0+(2392.0/128.0)*p),2523.0/32.0);
  }

static void near(double actual, double expected, double tolerance, const char* message) {
  if(!std::isfinite(actual) || std::abs(actual-expected)>tolerance)
    throw std::runtime_error(message);
  }

int main() {
  std::atomic_uint errors{0};
  Log::setOutputCallback([&](Log::Mode mode,const char* text) {
    std::cerr << text << '\n';
    if(mode==Log::Error) ++errors;
    });
  try {
    VulkanApi api{ApiFlags::Validation};
    Device device(api);
    auto pipeline = device.pipeline(device.shader("hdr-output.spv"));
    using Value = std::array<float,4>;
    std::vector<Value> values(4097*2);
    auto output = device.ssbo(Uninitialized,values.size()*sizeof(Value));
    auto cmd = device.commandBuffer();
    {
    auto enc = cmd.startEncoding(device);
    enc.setBinding(0,output);
    enc.setPipeline(pipeline);
    enc.dispatchThreads(4097,1);
    }
    auto fence = device.submit(cmd);
    fence.wait();
    device.readBytes(output,values.data(),values.size()*sizeof(Value));
    double previousHdr = 0;
    for(size_t i=0; i<4097; ++i) {
      const double x = double(i)/256.0;
      const double aces = std::clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0);
      const auto& a = values[i*2];
      const auto& b = values[i*2+1];
      near(a[0],std::pow(aces,1.0/2.2),0.00002,"SDR tone curve changed");
      const double t = std::max(x-1.0,0.0);
      near(a[1],std::pow(aces+4*t*t/(1+t*t),1.0/2.2),0.00002,"HDR highlight curve mismatch");
      if(x<=1) near(a[1],a[0],0.000002,"HDR changed darker scene values");
      if(a[1]+0.000002<previousHdr || a[1]>std::pow(5.0,1.0/2.2)+0.00002)
        throw std::runtime_error("HDR curve is not monotonic or exceeds peak");
      previousHdr = a[1];
      near(a[2],pq(x*200),0.00005,"Neutral PQ red mismatch");
      near(a[3],pq(x*200),0.00005,"Neutral PQ green mismatch");
      near(b[0],pq(x*200),0.00005,"Neutral PQ blue mismatch");
      near(b[1],pq(x*200*0.627404),0.00005,"Rec.2020 red mismatch");
      near(b[2],pq(x*200*0.069097),0.00005,"Rec.2020 green mismatch");
      near(b[3],pq(x*200*0.0163916),0.00005,"Rec.2020 blue mismatch");
      }
    // The SDR UI's encoded white must map to 200 nits, not the 1000-nit scene peak.
    near(values[256*2][2],pq(200),0.00005,"Paper white mismatch");
    near(pq(1000),0.7518270962,0.000000001,"ST 2084 reference mismatch");
    device.waitIdle();
    }
  catch(const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
    }
  if(errors!=0) return 1;
  std::cout << "4097 HDR/SDR samples passed, including PQ, gamut conversion and paper white\n";
  }
