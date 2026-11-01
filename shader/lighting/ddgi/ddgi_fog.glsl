#ifndef DDGI_FOG_GLSL
#define DDGI_FOG_GLSL

// Fog probe volume: view-aligned slices, quadratic in distance so the first meters get most of them
const float DdgiFogRange = 150.0; // meters

// far end of slice i, meters along the view ray
float ddgiFogSliceDist(int i, int slices) {
  const float t = float(i+1)/float(slices);
  return DdgiFogRange*t*t;
  }

// texture coordinate of distance r (meters), inverse of ddgiFogSliceDist
float ddgiFogSliceCoord(float r, int slices) {
  return sqrt(clamp(r/DdgiFogRange, 0.0, 1.0)) - 0.5/float(slices);
  }

#endif
