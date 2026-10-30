#version 450

#extension GL_GOOGLE_include_directive : enable

#include "antialiasing/smaa/smaa.glsl"

layout(binding = 0) uniform sampler2D edgesTex;
layout(binding = 1) uniform sampler2D areaTex;
layout(binding = 2) uniform sampler2D searchTex;

layout(location = 0) in  vec2 uv;
layout(location = 0) out vec4 outWeights;

void main() {
  vec2 pixcoord;
  vec4 offset[3];
  SMAABlendingWeightCalculationVS(uv, pixcoord, offset);
  outWeights = SMAABlendingWeightCalculationPS(uv, pixcoord, offset, edgesTex, areaTex, searchTex, vec4(0));
  }
