#ifndef CHARACTER_CAPSULES_GLSL
#define CHARACTER_CAPSULES_GLSL

// Characters are not in the ray tracing scene. Their bones as capsules let shadow rays still find them;
// the silhouette is rough, which torch light hides well. Built each frame by Renderer::prepareCharacterCapsules.
const uint MaxCapsuleGroups = 64;

struct CapsuleGroup {
  vec4 sphere; // bounds of the character: center and radius
  uint first;  // index of its first capsule
  uint count;
  uint padding0, padding1;
  };

layout(binding = CAPSULE_BINDING, std430) readonly buffer CharacterCapsules {
  uint         groupCount;
  uint         padding0, padding1, padding2;
  CapsuleGroup groups[MaxCapsuleGroups];
  vec4         capsules[]; // pairs: (start, radius), (end, unused)
  } characters;

// squared distance between segments p0-p1 and q0-q1, s is where on p0-p1 (Ericson, Real-Time Collision Detection 5.1.9)
float segmentDistanceSq(vec3 p0, vec3 p1, vec3 q0, vec3 q1, out float s) {
  const vec3  d1 = p1-p0;
  const vec3  d2 = q1-q0;
  const vec3  r  = p0-q0;
  const float a  = dot(d1,d1);
  const float e  = dot(d2,d2);
  const float f  = dot(d2,r);
  float t = 0;
  s = 0;
  if(a<=1e-6 && e<=1e-6)
    return dot(r,r);
  if(a<=1e-6) {
    t = clamp(f/e, 0.0, 1.0);
    } else {
    const float c = dot(d1,r);
    if(e<=1e-6) {
      s = clamp(-c/a, 0.0, 1.0);
      } else {
      const float b     = dot(d1,d2);
      const float denom = a*e - b*b;
      s = denom>1e-6 ? clamp((b*f - c*e)/denom, 0.0, 1.0) : 0.0;
      t = (b*s + f)/e;
      if(t<0) {
        t = 0;
        s = clamp(-c/a, 0.0, 1.0);
        }
      else if(t>1) {
        t = 1;
        s = clamp((b-c)/a, 0.0, 1.0);
        }
      }
    }
  const vec3 cp = p0 + d1*s;
  const vec3 cq = q0 + d2*t;
  return dot(cp-cq, cp-cq);
  }

float pointSegmentDistanceSq(vec3 p, vec3 a, vec3 b) {
  const vec3  ab = b-a;
  const float t  = clamp(dot(p-a, ab)/max(dot(ab,ab), 1e-6), 0.0, 1.0);
  const vec3  d  = a + ab*t - p;
  return dot(d,d);
  }

// does a character block the segment from a receiver at `from` to `to`; dist is how far from `from`
bool characterOccludes(vec3 from, vec3 to, out float dist) {
  dist = -1;
  const uint count = min(characters.groupCount, MaxCapsuleGroups);
  for(uint g=0; g<count; ++g) {
    const CapsuleGroup grp = characters.groups[g];
    const float r = grp.sphere.w;
    if(pointSegmentDistanceSq(grp.sphere.xyz, from, to) > r*r)
      continue;
    for(uint i=0; i<grp.count; ++i) {
      const vec4  c0 = characters.capsules[(grp.first+i)*2+0];
      const vec3  c1 = characters.capsules[(grp.first+i)*2+1].xyz;
      const float cr = c0.w;
      // a capsule the receiver lies in is its own body: a character does not shadow itself there
      if(pointSegmentDistanceSq(from, c0.xyz, c1) < cr*cr*1.44)
        continue;
      float s = 0;
      if(segmentDistanceSq(from, to, c0.xyz, c1, s) < cr*cr) {
        const float d = s*length(to-from);
        dist = dist<0 ? d : min(dist, d);
        }
      }
    }
  return dist>=0;
  }

bool characterOccludes(vec3 from, vec3 to) {
  float dist = 0;
  return characterOccludes(from, to, dist);
  }

#endif
