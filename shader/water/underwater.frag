#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "scene.glsl"
#include "common.glsl"

layout(location = 0) out vec4 outColor;

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1) uniform sampler2D zbuffer;
layout(binding = 2) uniform sampler2D gbufDiffuse;

layout(push_constant, std430) uniform Push {
  float waterY;
  } push;

// fixme: copy-paste
vec4 waterScatter(vec3 back, vec3 normal, const float len) {
  const vec3 transmittance = waterTransmittance(len);
#if defined(SCATTERING)
  const float f       = fresnel(scene.sunDir,normal,IorWater);
  const vec3  scatter = f * scene.sunColor * (1-exp(-len/20000.0)) * scene.exposure;
  return vec4(scatter*transmittance, 1);
#else
  return vec4(transmittance, 1);
#endif
  }

vec3 unproject(vec4 screen) {
  const vec4 pos4 = scene.viewProjectInv * screen;
  return pos4.xyz/pos4.w;
  }

void main() {
  const vec2  fragCoord = (gl_FragCoord.xy*scene.screenResInv)*2.0-vec2(1.0);
  const float depth     = texelFetch(zbuffer,  ivec2(gl_FragCoord.xy), 0).r;

  const vec3  camPos    = scene.camPos;
  const vec3  wPos      = unproject(vec4(fragCoord.x, fragCoord.y, depth, 1.0));

  const vec3 ray = wPos-camPos;
  float len = length(ray);
  if(camPos.y<push.waterY) {
    // The water exit surface can be clipped away by the near plane.
    if(ray.y>0.0)
      len *= clamp((push.waterY-camPos.y)/ray.y, 0.0, 1.0);
    } else {
    const vec3 nearPos = unproject(vec4(fragCoord,0.0,1.0));
    const float hint = texelFetch(gbufDiffuse,ivec2(gl_FragCoord.xy),0).a;
    // Visible water already shades its submerged background; only fill a clipped entry.
    if(nearPos.y>=push.waterY || ray.y>=0.0 || isGBufWater(hint))
      discard;
    len *= 1.0-clamp((push.waterY-camPos.y)/ray.y, 0.0, 1.0);
    }
  outColor = waterScatter(vec3(1),vec3(0,1,0),len);
  }
