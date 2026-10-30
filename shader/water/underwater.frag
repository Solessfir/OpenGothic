#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "scene.glsl"
#include "common.glsl"

layout(location = 0) out vec4 outColor;

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1) uniform sampler2D zbuffer;
layout(binding = 2) uniform sampler2D gbufDiffuse;
layout(binding = 3) uniform texture2D irradiance;

layout(push_constant, std430) uniform Push {
  float waterY;
  } push;

#include "water/water_haze.glsl"
#include "water/gerstner_wave.glsl"

vec3 unproject(vec4 screen) {
  const vec4 pos4 = scene.viewProjectInv * screen;
  return pos4.xyz/pos4.w;
  }

void main() {
  const vec2  fragCoord = (gl_FragCoord.xy*scene.screenResInv)*2.0-vec2(1.0);
  const float depth     = texelFetch(zbuffer,  ivec2(gl_FragCoord.xy), 0).r;

  const vec3  camPos    = scene.camPos;
  const vec3  wPos      = unproject(vec4(fragCoord.x, fragCoord.y, depth, 1.0));

  // Only views that start under the waves; with the camera in the surface, part of the screen looks out from the air
  if(nearPlaneAboveWater(gl_FragCoord.xy, push.waterY)>=0.0)
    discard;

  const vec3 ray = wPos-camPos;
  float len = length(ray);
  if(camPos.y<push.waterY) {
    // The water exit surface can be clipped away by the near plane.
    if(ray.y>0.0)
      len *= clamp((push.waterY-camPos.y)/ray.y, 0.0, 1.0);
    } else {
    const float hint = texelFetch(gbufDiffuse,ivec2(gl_FragCoord.xy),0).a;
    // Visible water already shades its submerged background; only fill a clipped entry.
    if(ray.y>=0.0 || isGBufWater(hint))
      discard;
    len *= 1.0-clamp((push.waterY-camPos.y)/ray.y, 0.0, 1.0);
    }
  // Two passes: one multiplies the scene by the transmittance, the other adds the haze the water scatters in
#if defined(SCATTERING)
  outColor = vec4(waterHaze()*(1.0-waterAbsorption(len)), 1);
#else
  outColor = vec4(waterAbsorption(len), 1);
#endif
  }
