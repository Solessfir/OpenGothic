#version 450

#extension GL_GOOGLE_include_directive : enable

#include "scene.glsl"

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1) uniform sampler2D blurred;

layout(push_constant, std430) uniform Push {
  float waterY;
  } push;

#include "water/gerstner_wave.glsl"

layout(location = 0) in  vec2 uv;
layout(location = 0) out vec4 outColor;

vec2 hash2(vec2 p) {
  p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
  return fract(sin(p)*43758.5453)*2.0-1.0;
  }

// Smooth value noise, two channels
vec2 noise2(vec2 p) {
  const vec2 i = floor(p);
  const vec2 f = fract(p);
  const vec2 u = f*f*(3.0-2.0*f);
  return mix(mix(hash2(i), hash2(i+vec2(1,0)), u.x),
             mix(hash2(i+vec2(0,1)), hash2(i+vec2(1,1)), u.x), u.y);
  }

void main() {
  // A subtle meniscus a few pixels wide where the waterline crosses the lens; above it the image stays sharp
  const float side = nearPlaneAboveWater(gl_FragCoord.xy, push.waterY);
  const float line = 1.0 - smoothstep(0.0, 4.0, abs(side)/max(fwidth(side), 1e-4));
  if(side>0.0 && line<=0.0)
    discard;

  const float time   = float(scene.tickCount32)/1000.0;
  const float aspect = float(scene.screenRes.x)/float(scene.screenRes.y);
  const vec2  p      = uv*vec2(aspect, 1.0);

  // Two slow wave layers drifting in different directions
  vec2 duv = uv;
  duv += noise2(p*4.0 + time*vec2( 0.9, 0.7)) * 0.006;
  duv += noise2(p*2.0*vec2(-0.7, 0.8) + time*vec2(-0.6, 0.8)) * 0.009;

  const vec3 c = textureLod(blurred, clamp(duv, vec2(0), vec2(1)), 0).rgb;
  outColor = vec4(c*(1.0-0.4*line), 1);
  }
