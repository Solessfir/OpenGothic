#ifndef SMAA_GLSL
#define SMAA_GLSL

layout(push_constant, std140) uniform PushConstant {
  vec4 rtMetrics;
  } push;

#define SMAA_GLSL_4 1
#define SMAA_RT_METRICS push.rtMetrics
#if defined(ULTRA)
#define SMAA_PRESET_ULTRA 1
#else
#define SMAA_PRESET_HIGH 1
#endif

#include "../../../lib/smaa/SMAA.hlsl"

#endif
