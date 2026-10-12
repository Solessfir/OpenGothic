#include "utils/workers.h"

#include <chrono>
#include <iostream>
#include <stdexcept>

int main() {
  try {
    for(size_t round=0; round<1000; ++round) {
      for(size_t count : {size_t(0),size_t(1),size_t(16),size_t(33)}) {
        std::vector<std::atomic_uint> calls(count);
        std::vector<size_t> results(count);
        Workers::parallelTasks(count,[&](size_t i) {
          if(i>=count)
            std::terminate();
          if(round%100==0 && i==0)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
          ++calls[i];
          results[i] = round+i+1;
          });
        for(size_t i=0; i<count; ++i)
          if(calls[i]!=1 || results[i]!=round+i+1)
            throw std::runtime_error("Lost, duplicated or incompletely published task");
        }
      for(size_t count : {size_t(0),size_t(1),size_t(127),size_t(128),size_t(129),size_t(512),size_t(4097)}) {
        std::vector<size_t> data(count,round);
        Workers::parallelFor(data,[](size_t& value) { ++value; });
        for(size_t value:data)
          if(value!=round+1)
            throw std::runtime_error("Parallel loop did not finish exactly once");
        }
      }
    std::cout << "11000 batches passed; delayed completion and result visibility checked\n";
    }
  catch(const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
    }
  // The singleton destructor must also wake and join every worker without hanging.
  return 0;
  }
