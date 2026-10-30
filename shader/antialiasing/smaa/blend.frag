#version 450

#extension GL_GOOGLE_include_directive : enable

#include "antialiasing/smaa/smaa.glsl"

layout(binding = 0) uniform sampler2D colorTex;
layout(binding = 1) uniform sampler2D blendTex;

layout(location = 0) in  vec2 uv;
layout(location = 0) out vec4 outColor;

void main() {
  vec4 offset;
  SMAANeighborhoodBlendingVS(uv, offset);
  outColor = SMAANeighborhoodBlendingPS(uv, offset, colorTex, blendTex);
  }
