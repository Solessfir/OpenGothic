#ifndef CAMERA_OBSTRUCTION_GLSL
#define CAMERA_OBSTRUCTION_GLSL

float cameraObstructionVisibilityAt(vec3 viewPos) {
  if(scene.cameraFadeFar2<=0.0)
    return 1.0;

  float visibility = smoothstep(scene.cameraFadeNear2,scene.cameraFadeFar2,dot(viewPos,viewPos));
  vec3 target = scene.cameraFadeTarget.xyz;
  float radius = scene.cameraFadeTarget.w;
  float length2 = dot(target,target);
  if(radius>0.0 && length2>1.0) {
    float t = dot(viewPos,target)/length2;
    if(t>0.0 && t<1.0) {
      // Every layer in the corridor shares a fully clear core, not cumulative partial opacity.
      vec3 offset = viewPos-target*t;
      float radial = smoothstep(radius*radius*0.5625,radius*radius,dot(offset,offset));
      // Close the corridor on the camera side of the player, never beyond them.
      float endFade = 1.0-smoothstep(0.0,20.0,(1.0-t)*sqrt(length2));
      visibility = min(visibility,max(radial,endFade));
      }
    }
  return visibility;
  }

float cameraObstructionVisibility(vec2 pixel, float depth) {
  vec2 ndc = pixel*scene.screenResInv*2.0-1.0;
  vec4 viewPos = scene.projectInv*vec4(ndc,depth,1.0);
  return cameraObstructionVisibilityAt(viewPos.xyz/viewPos.w);
  }

float cameraObstructionThreshold(ivec2 pixel) {
  // Fixed ordered coverage, with no frame-dependent noise or extra texture reads.
  const int bayer[16] = int[16](0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5);
  return (float(bayer[(pixel.y&3)*4+(pixel.x&3)])+0.5)/16.0;
  }

#endif
