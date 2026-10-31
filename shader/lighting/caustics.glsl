#ifndef CAUSTICS_GLSL
#define CAUSTICS_GLSL

// Expects `scene` to be declared before the include.

vec2 causticHash(vec2 p) {
  p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
  return fract(sin(p)*43758.5453);
  }

// Cells around moving centers: the gap between the two nearest centers is small along the cell borders, which form the web
float causticWeb(vec2 p, float t) {
  // Bend the straight cell borders, real caustics are curved
  p += 0.3*vec2(sin(p.y*1.7 + t), cos(p.x*1.3 - t*0.8));

  const vec2 i  = floor(p);
  const vec2 f  = fract(p);
  float      d1 = 8.0, d2 = 8.0;
  for(int y=-1; y<=1; ++y)
    for(int x=-1; x<=1; ++x) {
      const vec2  g = vec2(x, y);
      const vec2  o = 0.5 + 0.4*sin(t + 6.2831*causticHash(i+g));
      const float d = length(g + o - f);
      d2 = d<d1 ? d1 : min(d2, d);
      d1 = min(d1, d);
      }
  const float line = 1.0 - smoothstep(0.0, 0.2, d2-d1);
  return line*line;
  }

// Waves focus sunlight into moving bright lines on the bottom of water.
// Applied where the ground is seen through the water, so dry land never gets them.
float waterCaustics(vec3 pos, float level) {
  const float below = level - pos.y;
  // Only sunlight focuses; at night and in dusk they fade out
  const float day   = smoothstep(0.0, 0.2, scene.sunDir.y);
  if(below<=0.0 || day<=0.0)
    return 1.0;

  // The lines need a little water to focus and blur out with depth
  const float fade = smoothstep(0.0, 30.0, below) * exp(-below/1500.0) * day;
  if(fade<=0.001)
    return 1.0;

  const vec2  q = pos.xz*0.01; // meters
  const float t = float(scene.tickCount32%10000000u)*0.001*0.8;
  const float c = causticWeb(q*1.2, t) + 0.5*causticWeb(q*2.3 + 7.0, t*1.3);
  // The web averages about 0.26, so the base keeps the average light unchanged
  return mix(1.0, 0.8 + 0.75*c, fade);
  }

#endif
