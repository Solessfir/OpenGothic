#version 450
#extension GL_GOOGLE_include_directive : enable

#include "scene.glsl"

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1) uniform sampler2D shafts;

layout(location = 0) out vec4 outColor;

const float Strength = 0.5;

void main() {
  const vec2  uv = gl_FragCoord.xy*scene.screenResInv;
  const float s  = textureLod(shafts, uv, 0).r;
  outColor = vec4(scene.sunColor*(s*Strength*scene.exposure), 0);
  }
