#ifndef DDGI_LIGHTS_GLSL
#define DDGI_LIGHTS_GLSL

const uint DdgiMaxLights = 255;

struct DdgiLightList {
  uint count;
  uint id[DdgiMaxLights];
  };

#endif
