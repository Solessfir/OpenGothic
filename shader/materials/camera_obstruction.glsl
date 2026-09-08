#ifndef CAMERA_OBSTRUCTION_GLSL
#define CAMERA_OBSTRUCTION_GLSL

float cameraObstructionVisibility(vec2 pixel, float depth) {
  vec2 ndc = pixel*scene.screenResInv*2.0-1.0;
  vec4 viewPos = scene.projectInv*vec4(ndc,depth,1.0);
  float distance2 = dot(viewPos.xyz,viewPos.xyz)/(viewPos.w*viewPos.w);
  return smoothstep(scene.cameraFadeNear2,scene.cameraFadeFar2,distance2);
  }

float cameraObstructionThreshold(ivec2 pixel) {
  // Fixed ordered coverage, with no frame-dependent noise or extra texture reads.
  const int bayer[16] = int[16](0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5);
  return (float(bayer[(pixel.y&3)*4+(pixel.x&3)])+0.5)/16.0;
  }

#endif
