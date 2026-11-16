#pragma once

#include <cstdint>

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

  Sample simulation, record, submit, present, gpu;
  uint32_t skipped = 0;
  };
