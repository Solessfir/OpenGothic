#ifndef SHADOWSAMPLING_GLSL
#define SHADOWSAMPLING_GLSL

#include "scene.glsl"

vec4 shadowSample(in sampler2D shadowMap, vec2 shPos) {
  shPos.xy = shPos.xy*vec2(0.5,0.5)+vec2(0.5);
  return textureGather(shadowMap,shPos);
  }

vec4 shadowSample(in sampler2D shadowMap, vec2 shPos, out vec2 m) {
  shPos.xy = shPos.xy*vec2(0.5,0.5)+vec2(0.5);
  m        = fract(shPos.xy * textureSize(shadowMap, 0) - vec2(0.5));
  return textureGather(shadowMap,shPos);
  }

float shadowResolve(in vec4 sh, float z) {
  z  = max(0,z);
  sh = step(sh,vec4(z));
  return 0.25*(sh.x+sh.y+sh.z+sh.w);
  }

float shadowResolve(in vec4 sh, float z, vec2 m) {
  const float bias = 0.0002;
  z  = max(0, z);
  sh = step(sh, vec4(z));

  vec2 xx = mix(sh.wz, sh.xy, m.y);
  return    mix(xx.x,  xx.y,  m.x);
  }

float calcShadowMs4(in sampler2D shadowMap0, vec3 shPos0) {
  vec2  bias = vec2(1)/vec2(textureSize(shadowMap0, 0));
  vec4  lay0 = vec4(0);
  float ret  = 0;

  vec2  m0;
  lay0 = shadowSample(shadowMap0,shPos0.xy + bias*vec2(-1.5,  0.5), m0);
  ret += shadowResolve(lay0,shPos0.z,m0);
  lay0 = shadowSample(shadowMap0,shPos0.xy + bias*vec2( 0.5,  0.5), m0);
  ret += shadowResolve(lay0,shPos0.z,m0);
  lay0 = shadowSample(shadowMap0,shPos0.xy + bias*vec2(-1.5, -1.5), m0);
  ret += shadowResolve(lay0,shPos0.z,m0);
  lay0 = shadowSample(shadowMap0,shPos0.xy + bias*vec2( 0.5, -1.5), m0);
  ret += shadowResolve(lay0,shPos0.z,m0);

  return ret * 0.25;
  }

float calcShadow(in SceneDesc scene,
                 in sampler2D shadowMap0, vec3 shPos0,
                 in sampler2D shadowMap1, vec3 shPos1) {
  vec2 minMax = scene.closeupShadowSlice;
  vec2 m1;
  vec4 lay1   = shadowSample(shadowMap1,shPos1.xy, m1);
  lay1.x = max(max(lay1.x,lay1.y), max(lay1.z,lay1.w));

  // fades: near to far at the near border, far to lit at the far border, so neither leaves a line
  const float edge0 = max(abs(shPos0.x), abs(shPos0.y));
  const bool  near  = edge0<0.99 && lay1.x<minMax[1];
  const float w     = near ? smoothstep(0.8, 0.99, edge0) : 1.0;

  float ret = near ? calcShadowMs4(shadowMap0,shPos0)*(1.0-w) : 0.0;
  if(w>0) {
    const float edge1 = max(abs(shPos1.x), abs(shPos1.y));
    const float far   = edge1<1.0 ? mix(calcShadowMs4(shadowMap1,shPos1), 1.0, smoothstep(0.85, 1.0, edge1)) : 1.0;
    ret += far*w;
    }
  return ret;
  }

// Receiver position in a cascade, offset along the normal by half a texel of that cascade. Where the sun grazes
// a wall, any offset slides the shadow edges along it by offset/tan(angle), so a fixed offset in cm made the
// near and far cascades disagree and cut shadows at their border. Half a texel keeps acne away.
vec3 shadowCascadePos(in mat4 viewShadow, in sampler2D shadowMap, vec3 pos, vec3 normal) {
  const float clipPerCm = min(length(vec3(viewShadow[0][0], viewShadow[1][0], viewShadow[2][0])),
                              length(vec3(viewShadow[0][1], viewShadow[1][1], viewShadow[2][1])));
  const float texelCm   = 2.0/(float(textureSize(shadowMap,0).x)*clipPerCm);
  const vec3  p         = pos + normal*(0.5*texelCm);
  const vec4  sp        = viewShadow*vec4(p, 1.0);
  return sp.xyz/sp.w;
  }

float calcShadow(in vec3 pos, const vec3 normal, in SceneDesc scene, in sampler2D shadowMap0, in sampler2D shadowMap1) {
#if defined(LWC)
  const vec3 shPos0 = shadowCascadePos(scene.viewShadowLwc[0], shadowMap0, pos, normal);
  const vec3 shPos1 = shadowCascadePos(scene.viewShadowLwc[1], shadowMap1, pos, normal);
#else
  const vec3 shPos0 = shadowCascadePos(scene.viewShadow[0], shadowMap0, pos, normal);
  const vec3 shPos1 = shadowCascadePos(scene.viewShadow[1], shadowMap1, pos, normal);
#endif
  return calcShadow(scene, shadowMap0,shPos0, shadowMap1,shPos1);
  }

float calcShadow(in vec4 pos4, in float bias, in SceneDesc scene, in sampler2D shadowMap0, in sampler2D shadowMap1) {
  vec4 shadowPos[2];
  vec3 offset = bias*scene.sunDir*pos4.w;
#if defined(LWC)
  shadowPos[0] = scene.viewShadowLwc[0]*vec4(pos4.xyz + offset*1.0, pos4.w);
  shadowPos[1] = scene.viewShadowLwc[1]*vec4(pos4.xyz + offset*3.0, pos4.w);
#else
  shadowPos[0] = scene.viewShadow   [0]*vec4(pos4.xyz + offset*1.0, pos4.w);
  shadowPos[1] = scene.viewShadow   [1]*vec4(pos4.xyz + offset*3.0, pos4.w);
#endif

  vec3 shPos0  = (shadowPos[0].xyz)/shadowPos[0].w;
  vec3 shPos1  = (shadowPos[1].xyz)/shadowPos[1].w;
  return calcShadow(scene, shadowMap0,shPos0, shadowMap1,shPos1);
  }

#endif
