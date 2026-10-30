#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "lighting/rt/rt_common.glsl"
#include "lighting/tonemapping.glsl"
#include "scene.glsl"
#include "common.glsl"

layout(location = 0) out vec4 outColor;

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };

#include "gerstner_wave.glsl"

layout(binding = 1) uniform sampler2D  sceneColor;
layout(binding = 2) uniform sampler2D  gbufDiffuse;
layout(binding = 3) uniform usampler2D gbufNormal;
layout(binding = 4) uniform sampler2D  gbufDepth;
layout(binding = 5) uniform sampler2D  zbuffer;

// 6 to 11 belong to the ray-traced scene
layout(binding = 12) uniform sampler2D skyLUT;
layout(binding = 13) uniform texture2D irradiance;

#include "water/water_haze.glsl"
#include "lighting/caustics.glsl"

#if defined(RAY_QUERY)
vec3 skyIrradiance(vec3 n) {
  const ivec3 d = ivec3(n.x>=0 ? 1 : 0, n.y>=0 ? 1 : 0, n.z>=0 ? 1 : 0);
  n = n*n;
  return texelFetch(irradiance, ivec2(0,d.x), 0).rgb * n.x +
         texelFetch(irradiance, ivec2(1,d.y), 0).rgb * n.y +
         texelFetch(irradiance, ivec2(2,d.z), 0).rgb * n.z;
  }

bool isSunBlocked(vec3 pos) {
  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery, topLevelAS, RayFlagsShadow, CM_ShadowCaster, pos, 5.0, scene.sunDir, 5000*100);
  rayQueryProceedShadow(rayQuery);
  return rayQueryGetIntersectionTypeEXT(rayQuery, true)!=gl_RayQueryCommittedIntersectionNoneEXT;
  }

// The part of the mirror the screen can not show: trace the scene and light the hit the way the ambient and sun passes do
vec3 traceReflection(vec3 start, vec3 dir, vec3 sky) {
  // Under water the haze hides everything farther than 50 m
  const bool  underWater = scene.underWater!=0;
  const float maxDist    = underWater ? 5000.0 : 20000.0;

  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsCullBackFacingTrianglesEXT | gl_RayFlagsSkipAABBEXT, CM_ShadowCaster,
                        start, 1.0, dir, maxDist);
  rayQueryProceedAlphaTest(rayQuery);
  if(rayQueryGetIntersectionTypeEXT(rayQuery, true)==gl_RayQueryCommittedIntersectionNoneEXT)
    return underWater ? waterHaze() : sky;

  const float   rayT  = rayQueryGetIntersectionTEXT(rayQuery, true);
  const HitDesc hit   = pullCommitedHitDesc(rayQuery);
  const uint    id    = hit.instanceId;
  const uvec3   index = pullTrinagleIds(id, hit.primitiveId);
  const vec3    b     = hit.baryCoord;

  const vec2 uv = b.x*pullTexcoord(id,index.x) + b.y*pullTexcoord(id,index.y) + b.z*pullTexcoord(id,index.z);
  vec3       nr = b.x*pullNormal  (id,index.x) + b.y*pullNormal  (id,index.y) + b.z*pullNormal  (id,index.z);
  nr = normalize(mat3x3(rayQueryGetIntersectionObjectToWorldEXT(rayQuery, true))*nr);

  const vec3 albedo = textureAlbedo(textureLod(sampler2D(textures[nonuniformEXT(id)], smp), uv, 0).rgb);
  const vec3 pos    = start + dir*rayT;

  float sun = max(dot(nr, scene.sunDir), 0.0);
  if(sun>0.0 && isSunBlocked(pos + nr*2.0))
    sun = 0.0;

  const vec3 ambient = scene.ambient + skyIrradiance(nr)*0.8 + (nr.y*0.25+0.75)*NightAmbient*Fd_Lambert;
  const vec3 lit     = albedo*(scene.sunColor*sun*Fd_Lambert*waterCaustics(pos) + ambient)*scene.exposure;
  return underWater ? waterFog(lit, rayT) : lit;
  }
#endif

// What a mirror ray shows when the screen has nothing for it
vec3 reflectionMiss(vec3 start, vec3 refl, vec3 sky) {
#if defined(RAY_QUERY)
  if(scene.underWater!=0)
    return traceReflection(start, refl, sky);
#endif
  return sky;
  }

float intersectPlane(const vec3 pos, const vec3 dir, const vec4 plane) {
  float dist = dot(vec4(pos,1.0), plane);
  float step = -dot(plane.xyz,dir);
  if(abs(step)<=0.0)
    return 0;
  return dist/step;
  }

float intersectFrustum(const vec3 pos, const vec3 dir) {
  float ret = 0;
  for(int i=0; i<6; ++i) {
    // skip left and right planes to hide some SSR artifacts
    if(i==0 || i==1)
      continue;
    float len = intersectPlane(pos,dir,scene.frustrum[i]);
    if(len>0 && (len<ret || ret==0))
      ret = len;
    }
  return ret;
  }

vec3 sunBloom(vec3 refl) {
  const float sunSolidAngle  = 0.53*M_PI/180.0;
  const float minSunCosTheta = cos(sunSolidAngle);

  float cosTheta = dot(refl, scene.sunDir);
  if(cosTheta >= minSunCosTheta)
    return vec3(1.0);
  float offset = minSunCosTheta - cosTheta;
  float gaussianBloom = exp(-offset*50000.0)*0.5;
  float invBloom = 1.0/(0.02 + offset*300.0)*0.01;
  return vec3(gaussianBloom+invBloom);
  }

vec3 ssr(vec3 orig, vec3 start, vec3 refl, const float depth, vec3 sky) {
  const int   SSR_STEPS     = 64;
  const int   SSR_OCCLUSION = 50;
  const float ZBias         = 0.0;

  const float rayLen = intersectFrustum(start,refl);
  // return vec3(rayLen*0.01);
  if(rayLen<=0)
    return reflectionMiss(start, refl, sky);

  const vec4  dest4 = scene.viewProject*vec4(start+refl*rayLen, 1.0);
  const vec3  dest  = dest4.xyz/dest4.w;
  const float off   = interleavedGradientNoise(gl_FragCoord.xy);

  vec2  uv       = orig.xy*0.5+vec2(0.5);
  vec2  uvAlt    = uv;
  bool  found    = false;
  int   occluded = 0;
  float hitLen   = 0;
  // The first thing the ray passed behind, a better guess than the sky when nothing is found after it
  vec2  uvBehind = vec2(-1);

  for(int i=1; i<=SSR_STEPS; ++i) {
    const float t   = (float(i+off)/SSR_STEPS);
    const vec3  pos = mix(orig,dest,t*t);
    if(pos.z>=1.0)
      break;

    const vec2  p        = pos.xy*0.5+vec2(0.5);
    // Above the water rays may run past the sides to hide artifacts, from below that smears the screen edge
    if(scene.underWater!=0 && (p.x<0.0 || p.x>1.0 || p.y<0.0 || p.y>1.0))
      break;
    const float smpDepth = textureLod(zbuffer,p,0).r + ZBias;
    if(smpDepth==1.0)
      continue;

    if(smpDepth > pos.z)
      continue; // futher away, than expected

    vec4  hit4   = scene.viewProjectInv*vec4(pos.xy,smpDepth,1.0);
    vec3  hit    = /*normalize*/(hit4.xyz/hit4.w - start);
    float angCos = dot(hit,refl);

    if(angCos<0.0) {
      // screen-space data is occluded
      occluded++;
      uvAlt = p;
      continue;
      }

    // Bisect between the last step in front of the scene and this one: coarse steps leave stairs and stripes in the reflection
    float lo = float(i-1+off)/SSR_STEPS, hi = t;
    for(int k=0; k<6; ++k) {
      const float m  = 0.5*(lo+hi);
      const vec3  pm = mix(orig,dest,m*m);
      if(textureLod(zbuffer,pm.xy*0.5+vec2(0.5),0).r + ZBias <= pm.z)
        hi = m; else
        lo = m;
      }
    const vec3 ph = mix(orig,dest,hi*hi);

    // Screen depth has no thickness: far behind what the screen shows, the ray passes behind it, as over a lily pad lying on the water
    const float sceneZ = linearDepth(textureLod(zbuffer,ph.xy*0.5+vec2(0.5),0).r + ZBias, scene.clipInfo);
    if(linearDepth(ph.z, scene.clipInfo) - sceneZ > 25.0 + 0.02*sceneZ) {
      if(uvBehind.x<0.0)
        uvBehind = ph.xy*0.5+vec2(0.5);
      continue;
      }

    uv     = ph.xy*0.5+vec2(0.5);
    found  = true;
    hitLen = length(hit);
    break;
    }

  // return sky;
  // Under water the miss goes to the traced or hazy fallback instead
  if(!found && uvBehind.x>=0.0 && scene.underWater==0) {
#if defined(RAY_QUERY)
    // Behind it is what the ray really hits, often the shore under the lily pads
    return traceReflection(start, refl, sky);
#else
    uv    = uvBehind;
    found = true;
#endif
    }
  if(!found)
    uv = uvAlt;

  const vec3  reflection = textureLod(sceneColor,uv,0).rgb;
  const float edge       = scene.underWater!=0 ? min(min(uv.x, 1.0-uv.x), min(uv.y, 1.0-uv.y)) : uv.y;
  const float att        = smoothstep(0, scene.underWater!=0 ? 0.04 : 0.1, edge);

  if(found) {
    // From below the mirrored ray runs through water too
    const vec3 c = scene.underWater!=0 ? waterFog(reflection, hitLen) : reflection;
    if(att>=1.0)
      return c;
    return mix(reflectionMiss(start, refl, sky),c,att);
    }

  // Shoreline occlusion only makes sense above the water
  if(occluded>SSR_OCCLUSION && scene.underWater==0) {
    const float z  = texelFetch(zbuffer, ivec2(gl_FragCoord.xy), 0).r;

    const vec3 zs = projectiveUnproject(scene.projectInv, vec3(orig.xy, z    ));
    const vec3 zb = projectiveUnproject(scene.projectInv, vec3(orig.xy, depth));

    float h = length(zs - zb)*0.1;
    // float h = 1;
    h = 0.1 + 0.9*smoothstep(0, 0.25, h*0.005);
    return sky*h;
    //return vec3(h);
    }

  return reflectionMiss(start, refl, sky);
  }

#if defined(SSR)
vec3 reflection(vec3 orig, vec3 start, vec3 refl, const float depth, vec3 sky) {
  return ssr(orig,start,refl,depth,sky);
  }
#else
vec3 reflection(vec3 orig, vec3 start, vec3 refl, const float depth, vec3 sky) {
  return sky;
  }
#endif

void main() {
  const vec2  fragCoord = (gl_FragCoord.xy*scene.screenResInv)*2.0-vec2(1.0);
  const vec4  diff      = texelFetch(gbufDiffuse, ivec2(gl_FragCoord.xy), 0);

  if(!isGBufWater(diff.a))
    discard;

  const bool  underWater = (scene.underWater!=0);
  const float ior        = (underWater ? IorAir : IorWater);

  const float depth   = texelFetch (gbufDepth,  ivec2(gl_FragCoord.xy), 0).r;
  const vec3  normal  = normalFetch(gbufNormal, ivec2(gl_FragCoord.xy));

  const vec4  start4  = scene.viewProjectInv*vec4(fragCoord.x, fragCoord.y, depth, 1.0);
  const vec3  start   = start4.xyz/start4.w;

  const vec4  scr     = vec4(fragCoord.x, fragCoord.y, depth, 1.0);
  const vec3  camPos  = scene.camPos;

  const vec3  view    = normalize(start - camPos);
        vec3  refl    = reflect(view, normal);
  if(refl.y<0 && !underWater) {
    refl.y = 0;
    refl   = normalize(refl);
    }

  // From below, total internal reflection depends on the incoming view ray
  const float f = underWater ? 1.0-underwaterWindow(view,normal) : fresnel(refl,normal,ior);
  if(f<=0.0001)
    discard;

  // Rays that find nothing on screen mirror open water, which fades to the haze like any distant water
  vec3 sky = waterHaze();
  if(!underWater) {
    sky = vec3(0);
    vec3 sun = sunBloom(refl) * scene.sunColor;
    sky += textureSkyLUT(skyLUT, vec3(0,RPlanet,0), refl, scene.sunDir) * scene.GSunIntensity;
    sky *= scene.exposure;
    }

  // Total internal reflection loses nothing, only a reflection above the water takes the water's tint
  vec3 r = reflection(scr.xyz/scr.w,start,refl,depth,sky) * (underWater ? vec3(1) : WaterAlbedo) * f;
  // vec3  r = reflection(scr,start,refl,depth,sky);

  outColor = vec4(r, 0.0);
  //outColor = vec4(sky, 0.0);
  //outColor = vec4(normal, 0.0);
  }
