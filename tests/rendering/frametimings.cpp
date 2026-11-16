#include "../../common/utils/frametimings.h"

#include <cmath>
#include <iostream>

int main() {
  FrameTimings stats;
  if(stats.gpu.count!=0 || stats.gpu.average()!=0)
    return 1;

  // GPU-busy retries simulate without submitting another frame.
  stats.simulation.add(2);
  ++stats.skipped;
  stats.simulation.add(4);
  stats.record.add(8);
  stats.submit.add(1);
  stats.present.add(3);
  stats.gpu.add(25);
  stats.simulation.add(6);
  stats.record.add(10);
  stats.submit.add(1);
  stats.present.add(5);
  if(stats.simulation.count!=3 || stats.record.count!=2 || stats.skipped!=1 ||
     stats.simulation.average()!=4 || stats.record.average()!=9 || stats.present.average()!=4 ||
     stats.gpu.count!=1 || stats.gpu.average()!=25) {
    std::cerr << "Timing averages mixed simulation attempts, rendered frames or GPU samples\n";
    return 1;
    }

  stats = {};
  if(stats.skipped!=0 || stats.simulation.count!=0 || stats.gpu.count!=0 ||
     !std::isfinite(stats.record.average()) || stats.record.average()!=0) {
    std::cerr << "Timing window retained old samples\n";
    return 1;
    }
  }
