#version 450

#extension GL_GOOGLE_include_directive : enable

layout(binding = 0) uniform sampler2D scene;

layout(location = 0) in  vec2 uv;
layout(location = 0) out vec4 outColor;

// Quarter resolution: four bilinear taps average the 4x4 source block
void main() {
  const vec2 texel = 1.0/vec2(textureSize(scene, 0));
  vec3 c = vec3(0);
  c += textureLod(scene, uv + vec2(-1,-1)*texel, 0).rgb;
  c += textureLod(scene, uv + vec2( 1,-1)*texel, 0).rgb;
  c += textureLod(scene, uv + vec2(-1, 1)*texel, 0).rgb;
  c += textureLod(scene, uv + vec2( 1, 1)*texel, 0).rgb;
  outColor = vec4(c*0.25, 1);
  }
