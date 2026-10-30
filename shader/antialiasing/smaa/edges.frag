#version 450

#extension GL_GOOGLE_include_directive : enable

#include "antialiasing/smaa/smaa.glsl"

layout(binding = 0) uniform sampler2D colorTex;

layout(location = 0) in  vec2 uv;
layout(location = 0) out vec2 outEdges;

void main() {
  vec4 offset[3];
  SMAAEdgeDetectionVS(uv, offset);
  outEdges = SMAAColorEdgeDetectionPS(uv, offset, colorTex);
  }
