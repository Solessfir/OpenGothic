#include <Tempest/VulkanApi>
#include <Tempest/Device>
#include <Tempest/Fence>
#include <Tempest/Log>

#include <array>
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace Tempest;

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
    auto reference = device.pipeline(device.shader("ssao-reference.spv"));
    auto optimized = device.pipeline(device.shader("ssao-optimized.spv"));
    constexpr size_t groups = 1024;
    using Result = std::array<uint32_t,4>;
    std::vector<Result> a(groups*64), b(a.size());
    auto outputA = device.ssbo(Uninitialized,a.size()*sizeof(Result));
    auto outputB = device.ssbo(Uninitialized,b.size()*sizeof(Result));
    auto cmd = device.commandBuffer();
    uint64_t before = 0, after = 0;
    for(uint32_t seed=0; seed<16; ++seed) {
      {
      auto enc = cmd.startEncoding(device);
      enc.setPushData(seed);
      enc.setBinding(0,outputA);
      enc.setPipeline(reference);
      enc.dispatchThreads(uint32_t(groups*8),8);
      enc.setBinding(0,outputB);
      enc.setPipeline(optimized);
      enc.dispatchThreads(uint32_t(groups*8),8);
      }
      auto fence = device.submit(cmd);
      fence.wait();
      device.readBytes(outputA,a.data(),a.size()*sizeof(Result));
      device.readBytes(outputB,b.data(),b.size()*sizeof(Result));
      for(size_t i=0; i<a.size(); ++i) {
        if(a[i][0]!=b[i][0] || a[i][1]!=b[i][1] || a[i][3]!=b[i][3] ||
           b[i][2]>a[i][2] || b[i][2]==0)
          throw std::runtime_error("SSAO convergence changed evaluated samples");
        if(b[i][2]!=b[(i/64)*64][2])
          throw std::runtime_error("SSAO stopping decision was not uniform");
        const auto kind = (i/64+seed)%16;
        if(kind==0 && (b[i][0]!=1 || b[i][2]!=1))
          throw std::runtime_error("Zero difference did not converge immediately");
        if(kind==1 && (b[i][0]!=8 || b[i][2]!=8))
          throw std::runtime_error("Unconverged group lost samples");
        before += a[i][2];
        after += b[i][2];
        }
      }
    if(after>=before)
      throw std::runtime_error("Converged groups did not avoid redundant rounds");
    device.waitIdle();
    std::cout << a.size()*16 << " lane cases passed; synchronization rounds "
              << before << " -> " << after << '\n';
    }
  catch(const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
    }
  if(errors!=0) {
    std::cerr << "Vulkan validation or engine errors occurred\n";
    return 1;
    }
  return 0;
  }
