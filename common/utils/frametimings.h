#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct FrameTimings {
  struct Sample {
    double   totalMs = 0;
    uint32_t count = 0;

    void add(double milliseconds) {
      totalMs += milliseconds;
      ++count;
      }

    double average() const {
      return count==0 ? 0 : totalMs/double(count);
      }
    };

  struct Pass {
    std::string name;
    Sample      time;
    };

  Sample simulation, record, submit, present, gpu;
  // GPU time by debug marker, so a report shows which pass is slow on a GPU nobody here owns
  std::vector<Pass> passes;
  uint32_t skipped = 0;

  void addPass(std::string_view name, double milliseconds) {
    for(auto& p:passes)
      if(p.name==name) {
        p.time.add(milliseconds);
        return;
        }
    passes.push_back({std::string(name),{}});
    passes.back().time.add(milliseconds);
    }
  };
