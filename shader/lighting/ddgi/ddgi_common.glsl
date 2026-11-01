#ifndef DDGI_COMMON_GLSL
#define DDGI_COMMON_GLSL

// World-space irradiance probe grids around the camera, cascades at 4x the spacing of the previous one.
// Probes live in toroidal arrays: grid coordinate g of cascade c is stored in slot c*DdgiCount + (g mod DdgiDims),
// so scrolling only resets the probes that enter a grid.

const ivec3 DdgiDims       = ivec3(32,16,32);
const int   DdgiCount      = DdgiDims.x*DdgiDims.y*DdgiDims.z;
const int   DdgiCascades   = 3;
const int   DdgiTotal      = DdgiCount*DdgiCascades;
const int   DdgiRays       = 64;
const int   DdgiIrrTile    = 8;  // 6x6 octahedral texels plus a 1 texel border for bilinear filtering
const int   DdgiDistTile   = 16; // 14x14 plus border, coarser lets light through thin roofs
const int   DdgiAtlasTiles = 128;
const float DdgiSpacing0   = 250;
const float DdgiRayLength  = 100*100;
const float DdgiDistMax    = 1.5*1.7320508; // distance moments are stored in probe spacing units

const uint  DdgiValid      = 0x1;
const uint  DdgiActive     = 0x2;

struct DdgiProbe {
  ivec3 coord;
  uint  flags;
  vec4  offset; // relocation out of geometry, world units
  };

struct DdgiSchedule {
  uvec3 dispatch; // indirect dispatch: one workgroup per listed probe
  uint  padding;
  int   slot[DdgiTotal];
  };

#define DDGI_PUSH                                       \
  layout(push_constant, std430) uniform UboPush {       \
    ivec4 origin[DdgiCascades];                         \
    uint  frame;                                        \
    uint  reset; /* lighting jumped: refresh every probe */ \
    } push;

float ddgiSpacing(int c) {
  return DdgiSpacing0*float(1<<(2*c));
  }

// far cascades see slow changes, they are traced less often
int ddgiUpdateRate(int c) {
  return c==0 ? 8 : (c==1 ? 32 : 64);
  }

ivec3 ddgiMod(ivec3 v) {
  return ((v % DdgiDims) + DdgiDims) % DdgiDims;
  }

int ddgiSlot(int c, ivec3 g) {
  const ivec3 s = ddgiMod(g);
  return c*DdgiCount + s.x + s.y*DdgiDims.x + s.z*DdgiDims.x*DdgiDims.y;
  }

ivec3 ddgiGridOf(int slot, ivec3 origin) {
  const int   l = slot % DdgiCount;
  const ivec3 s = ivec3(l % DdgiDims.x, (l / DdgiDims.x) % DdgiDims.y, l / (DdgiDims.x*DdgiDims.y));
  return origin + ddgiMod(s - origin);
  }

bool ddgiIsCurrent(in DdgiProbe p, ivec3 g) {
  return (p.flags & DdgiValid)!=0 && p.coord==g;
  }

bool ddgiIsScheduled(int slot, uint frame) {
  const int c    = slot / DdgiCount;
  const int rate = ddgiUpdateRate(c);
  return uint(slot % rate)==(frame % uint(rate));
  }

vec2 ddgiSignNotZero(vec2 v) {
  return vec2(v.x>=0 ? 1 : -1, v.y>=0 ? 1 : -1);
  }

vec2 ddgiOctEncode(vec3 n) {
  n /= (abs(n.x)+abs(n.y)+abs(n.z));
  vec2 p = n.xz;
  if(n.y<0)
    p = (1.0-abs(p.yx))*ddgiSignNotZero(p);
  return p;
  }

vec3 ddgiOctDecode(vec2 p) {
  vec3 n = vec3(p.x, 1.0-abs(p.x)-abs(p.y), p.y);
  if(n.y<0)
    n.xz = (1.0-abs(n.zx))*ddgiSignNotZero(n.xz);
  return normalize(n);
  }

// ray results: 128 probes of DdgiRays texels per row, a column of probes would exceed the max texture height
ivec2 ddgiRayCoord(int slot, int ray) {
  return ivec2((slot % DdgiAtlasTiles)*DdgiRays + ray, slot / DdgiAtlasTiles);
  }

ivec2 ddgiTileOrigin(int slot, int tile) {
  return ivec2(slot % DdgiAtlasTiles, slot / DdgiAtlasTiles)*tile;
  }

vec2 ddgiAtlasUv(int slot, vec3 dir, int tile, vec2 atlasSize) {
  const vec2 oct = ddgiOctEncode(dir)*0.5+0.5;
  const vec2 px  = vec2(ddgiTileOrigin(slot,tile)) + 1.0 + oct*float(tile-2);
  return px/atlasSize;
  }

// Direction of a tile texel; border texels repeat the mirrored interior texel, so bilinear filtering wraps correctly
vec3 ddgiTexelDir(ivec2 t, int tile) {
  const int  n  = tile-1;
  const bool bx = (t.x==0 || t.x==n);
  const bool by = (t.y==0 || t.y==n);
  ivec2 s = t;
  if(bx && by)
    s = ivec2(t.x==0 ? n-1 : 1, t.y==0 ? n-1 : 1);
  else if(by)
    s = ivec2(n-t.x, t.y==0 ? 1 : n-1);
  else if(bx)
    s = ivec2(t.x==0 ? 1 : n-1, n-t.y);
  const vec2 oct = (vec2(s-1)+0.5)/float(tile-2)*2.0-1.0;
  return ddgiOctDecode(oct);
  }

vec3 ddgiRandomRotation(uint frame, vec3 v) {
  // uniform random quaternion (Shoemake)
  uint  seed = frame*747796405u + 2891336453u;
  float u1 = float(wangHash(seed) & 0xFFFFFF)/16777216.0;
  float u2 = float(wangHash(seed) & 0xFFFFFF)/16777216.0*2.0*M_PI;
  float u3 = float(wangHash(seed) & 0xFFFFFF)/16777216.0*2.0*M_PI;
  vec4  q  = vec4(sqrt(1-u1)*sin(u2), sqrt(1-u1)*cos(u2), sqrt(u1)*sin(u3), sqrt(u1)*cos(u3));
  return v + 2.0*cross(q.xyz, cross(q.xyz, v) + q.w*v);
  }

vec3 ddgiRayDir(int ray, uint frame) {
  return ddgiRandomRotation(frame, sampleSphere(uint(ray), uint(DdgiRays), 0));
  }

#endif
