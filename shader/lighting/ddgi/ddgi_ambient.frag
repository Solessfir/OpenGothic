#version 460
#extension GL_ARB_separate_shader_objects       : enable
#extension GL_GOOGLE_include_directive          : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "lighting/tonemapping.glsl"
#include "scene.glsl"
#include "common.glsl"

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1) uniform texture2D  gbufDiffuse;
layout(binding = 2) uniform usampler2D gbufNormal;
layout(binding = 3) uniform sampler2D  giHalf; // rgb: irradiance/PI, a: linear depth
layout(binding = 4) uniform sampler2D  ssao;
layout(binding = 5) uniform sampler2D  depth;

layout(location = 0) out vec4 outColor;

// Same depth-aware 3x3 resolve as the half-resolution SSAO
vec3 resolveHalf(ivec2 pixel, float depth) {
  const ivec2 last   = textureSize(giHalf, 0)-1;
  const ivec2 base   = pixel/2;
  const vec2  center = (vec2(pixel)+0.5)*0.5-0.5;
  const float tolerance = max(1.0, abs(depth)*0.02);
  vec3  sum     = vec3(0);
  float weights = 0;
  vec3  nearest = vec3(0);
  float nearestDz = 1e20;
  for(int y=-1; y<=1; ++y)
    for(int x=-1; x<=1; ++x) {
      const ivec2 at     = clamp(base+ivec2(x,y), ivec2(0), last);
      const vec4  s      = texelFetch(giHalf, at, 0);
      const vec2  delta  = vec2(at)-center;
      const float dz     = (s.a-depth)/tolerance;
      const float weight = exp2(-0.5*dot(delta,delta)-dz*dz);
      sum     += s.rgb*weight;
      weights += weight;
      if(abs(dz)<nearestDz) {
        nearestDz = abs(dz);
        nearest   = s.rgb;
        }
      }
  // a surface missing from the half-resolution buffer takes the closest depth match
  return weights>0.0001 ? sum/weights : nearest;
  }

void main() {
  const ivec2 fragCoord = ivec2(gl_FragCoord.xy);
  const vec2  uv        = gl_FragCoord.xy*scene.screenResInv;

  const vec3  diff = texelFetch(gbufDiffuse, fragCoord, 0).rgb;
  const vec3  norm = normalFetch(gbufNormal, fragCoord);
  const float z    = texelFetch(depth, fragCoord, 0).x;

  vec3 luminance = resolveHalf(fragCoord, linearDepth(z, scene.clipInfo));
  luminance += (norm.y*0.25+0.75) * NightAmbient * Fd_Lambert;

  vec3 color = textureAlbedo(diff);
  color *= luminance;
  color *= textureLod(ssao, uv, 0).r;
  color *= scene.exposure;
  outColor = vec4(color, 1);
  }
