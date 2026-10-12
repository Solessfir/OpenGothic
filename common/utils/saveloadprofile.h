#pragma once

#include <Tempest/Log>
#include <chrono>
#include <cstdint>
#include <filesystem>

namespace SaveLoadProfile {

using Clock = std::chrono::steady_clock;

inline bool enabled() {
  static const bool value = [] {
    std::error_code error;
    return std::filesystem::exists("profile-save-load", error);
    }();
  return value;
  }

inline uint64_t now() {
  if(!enabled())
    return 0;
  return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count());
  }

inline void report(const char* label, uint64_t ns) {
  if(enabled())
    Tempest::Log::i("[SaveLoad] ", label, " ms=", double(ns)/1000000.0);
  }

inline uint64_t operationStart = 0;
inline const char* operation = "";

inline void begin(const char* label) {
  operation = label;
  operationStart = now();
  }

inline void finish() {
  if(operationStart!=0) {
    report(operation, now()-operationStart);
    operationStart = 0;
    }
  }

class Timer {
  public:
    explicit Timer(const char* label) : label(label), start(now()), last(start) {}
    ~Timer() { report(label, now()-start); }

    void step(const char* phase) {
      const auto time = now();
      report(phase, time-last);
      last = now();
      }

  private:
    const char* label;
    uint64_t start;
    uint64_t last;
  };

class Accumulate {
  public:
    explicit Accumulate(uint64_t& total) : total(total), start(now()) {}
    ~Accumulate() { total += now()-start; }

  private:
    uint64_t& total;
    uint64_t start;
  };

}
