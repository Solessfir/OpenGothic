#ifndef DDGI_SAMPLE_GLSL
#define DDGI_SAMPLE_GLSL

#include "lighting/ddgi/ddgi_common.glsl"

// Requires DDGI_PUSH, `DdgiProbe ddgiProbes[]` in a buffer, `ddgiIrradiance` and `ddgiDistance` samplers.

// One cascade: irradiance/PI at P, w is the cascade's coverage (0 outside, fading to 0 at its border)
vec3 ddgiSampleCascade(int c, vec3 P, vec3 N, vec3 V, out float w) {
  const float spacing = ddgiSpacing(c);
  const ivec3 origin  = push.origin[c].xyz;
  const vec3  biasP   = P + (N*0.2 + V*0.8)*(0.2*spacing);
  const vec3  gridP   = biasP/spacing;

  const vec3  inside  = min(gridP - vec3(origin), vec3(origin+DdgiDims-1) - gridP);
  const float border  = clamp(min(inside.x, min(inside.y, inside.z))*0.5, 0.0, 1.0);
  w = 0;
  if(border<=0)
    return vec3(0);

  const vec2  irrSize  = vec2(textureSize(ddgiIrradiance, 0));
  const vec2  distSize = vec2(textureSize(ddgiDistance,   0));
  const ivec3 baseG    = ivec3(floor(gridP));
  const vec3  alpha    = gridP - vec3(baseG);

  vec3  sum  = vec3(0);
  float wsum = 0;
  for(int i=0; i<8; ++i) {
    const ivec3 offset = ivec3(i&1, (i>>1)&1, (i>>2)&1);
    const ivec3 g      = baseG + offset;
    const int   slot   = ddgiSlot(c, g);
    const DdgiProbe p  = ddgiProbes[slot];
    if(!ddgiIsCurrent(p, g) || (p.flags & DdgiActive)==0)
      continue;

    const vec3  probeP    = vec3(g)*spacing + p.offset.xyz;
    const vec3  tri       = mix(1.0-alpha, alpha, vec3(offset));
    const vec3  toProbe   = normalize(probeP - P);

    float weight = (dot(toProbe, N)+1.0)*0.5;
    weight = weight*weight + 0.2;

    const vec3  probeToP  = biasP - probeP;
    const float len       = max(length(probeToP), 0.001);
    const float dist      = len/spacing;
    const vec2  moments   = textureLod(ddgiDistance, ddgiAtlasUv(slot, probeToP/len, DdgiDistTile, distSize), 0).rg;
    if(dist>moments.x) {
      const float variance = abs(moments.y - moments.x*moments.x);
      const float d        = dist - moments.x;
      float cheb = variance/(variance + d*d);
      cheb = cheb*cheb*cheb;
      weight *= max(0.05, cheb);
      }

    weight = max(0.000001, weight);
    if(weight<0.2)
      weight *= weight*weight/(0.2*0.2);
    weight *= tri.x*tri.y*tri.z;

    sum  += textureLod(ddgiIrradiance, ddgiAtlasUv(slot, N, DdgiIrrTile, irrSize), 0).rgb * weight;
    wsum += weight;
    }

  if(wsum<=0)
    return vec3(0);
  w = border;
  return sum/wsum;
  }

// V points from the surface to the viewer. Returns irradiance/PI, w is 0 outside of all grids.
// The finest cascade that covers P wins, coarser ones fill its border fade.
vec3 ddgiSampleIrradiance(vec3 P, vec3 N, vec3 V, out float w) {
  vec3  acc  = vec3(0);
  float rest = 1;
  for(int c=0; c<DdgiCascades && rest>0.001; ++c) {
    float wc = 0;
    const vec3 s = ddgiSampleCascade(c, P, N, V, wc);
    acc  += s*wc*rest;
    rest *= 1.0-wc;
    }
  w = 1.0-rest;
  return w>0 ? acc/w : vec3(0);
  }

#endif
