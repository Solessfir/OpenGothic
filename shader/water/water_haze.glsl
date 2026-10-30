#ifndef WATER_HAZE_GLSL
#define WATER_HAZE_GLSL

// Expects `scene` and the sky irradiance texture or sampler `irradiance` to be declared before the include.

// Share of the light falling on the water that scatters back toward the eye
const vec3 WaterScatter = vec3(0.03, 0.08, 0.10);

// Color that distance under water fades to. The water itself scatters light, so far water stays lit instead of turning black.
vec3 waterHaze() {
  // A white surface facing up, lit the same way the ambient and sun passes light the bottom
  const vec3 sky = texelFetch(irradiance, ivec2(1,1), 0).rgb;
  const vec3 up  = scene.ambient + sky*0.8 + NightAmbient*Fd_Lambert;
  const vec3 sun = scene.sunColor * max(scene.sunDir.y, 0.0) * Fd_Lambert;
  return (up + sun) * WaterScatter * scene.exposure;
  }

// The same water seen from above and from below: red goes first, then green; (0.16, 0.06, 0.035) per meter
vec3 waterAbsorption(float len) {
  return exp(-vec3(0.16, 0.06, 0.035) * (len/100.0));
  }

vec3 waterFog(vec3 color, float len) {
  const vec3 t = waterAbsorption(len);
  return color*t + waterHaze()*(1.0-t);
  }

#endif
