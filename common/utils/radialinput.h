#pragma once

#include <cmath>
#include <numbers>

namespace RadialInput {

inline int sector(float x, float y, float inner, float outer) {
  const float distance=std::hypot(x,y);
  if(distance<inner || distance>outer) return -1;
  float angle=std::atan2(x,-y);
  if(angle<0) angle+=2.f*std::numbers::pi_v<float>;
  return int(std::floor(angle/(std::numbers::pi_v<float>/4.f)+0.5f))%8;
  }

}
