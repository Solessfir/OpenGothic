#pragma once

#include <Tempest/Log>
#include <cstdint>
#include <fstream>
#include <string>

#if defined(__linux__)
#include <unistd.h>
#endif

// Process resident memory excludes GPU allocations on most phones.
inline std::string memoryInfo() {
#if defined(__linux__)
  std::ifstream statm("/proc/self/statm");
  uint64_t size = 0, resident = 0;
  if(!(statm >> size >> resident))
    return std::string();
  std::string info = " process MB="+std::to_string(resident*uint64_t(sysconf(_SC_PAGESIZE))/(1024*1024));
  std::ifstream meminfo("/proc/meminfo");
  std::string   key;
  uint64_t      kb = 0;
  while(meminfo >> key >> kb) {
    if(key=="MemAvailable:")
      return info+" available MB="+std::to_string(kb/1024);
    meminfo.ignore(64,'\n');
    }
  return info;
#else
  return std::string();
#endif
  }

template<class... Args>
inline void logMemoryInfo(const char* phase, const Args&... details) noexcept {
  try {
    const auto mem = memoryInfo();
    if(!mem.empty() || sizeof...(details)>0)
      Tempest::Log::i(phase,mem,details...);
    }
  catch(...) {
    // Diagnostics must not turn a successful save or its low-memory fallback into a failure.
    }
  }
