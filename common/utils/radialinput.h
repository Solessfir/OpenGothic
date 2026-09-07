#pragma once

#include <cmath>
#include <algorithm>
#include <numbers>

namespace RadialInput {

inline int sector(float x, float y, float inner, float outer, int count=8) {
  const float distance=std::hypot(x,y);
  if(count<=0 || distance<inner || distance>outer) return -1;
  float angle=std::atan2(x,-y);
  if(angle<0) angle+=2.f*std::numbers::pi_v<float>;
  return int(std::floor(angle/(2.f*std::numbers::pi_v<float>/float(count))+0.5f))%count;
  }

struct TouchLayout {
  float x=0, y=0;
  float radius=0, outer=0;
  int cell=0, footer=0;
  };

inline TouchLayout touchLayout(int width, int height, float scale, float anchorX, float anchorY, int choices, int lineHeight) {
  if(width<=0 || height<=0) return {};
  const float side=float(std::min(width,height));
  const float margin=std::min(std::max(8.f,side*0.025f),side*0.1f);
  const int footer=int(std::min(float(3*lineHeight)+margin,side*0.25f));
  float cell=std::min(64.f*std::max(0.1f,scale),side*0.115f);
  float radius=choices<=2 ? cell*1.05f : std::max(cell*1.6f,side*0.17f);
  float outer=radius+cell*0.6f;
  const float available=std::min(float(width)-2*margin,float(height-footer)-3*margin)*0.5f;
  const float fit=std::min(1.f,available/outer);
  cell*=fit; radius*=fit; outer*=fit;
  return {std::clamp(anchorX,outer+margin,float(width)-outer-margin),
          std::clamp(anchorY,outer+margin,float(height-footer)-outer-2*margin),
          radius,outer,std::max(1,int(cell)),footer};
  }

}
