#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// Detect fresh input, not a held stick or button left over from touch takeover.
class GamepadActivity {
  public:
    bool update(bool connected, uint32_t buttons, float lx, float ly, float rx, float ry,
                float lt, float rt, float deadZone, float triggerPress, float triggerRelease) {
      if(!connected) {
        available=false;
        previous=0;
        return false;
        }
      uint64_t next=buttons;
      const float threshold=std::max(0.15f,deadZone);
      auto axis=[&](float value, unsigned bit, float press, float release) {
        if(value>((previous&(uint64_t(1)<<bit))!=0 ? release : press))
          next|=uint64_t(1)<<bit;
        };
      axis(std::hypot(lx,ly),32,threshold,threshold*0.7f);
      axis(std::hypot(rx,ry),33,threshold,threshold*0.7f);
      axis(lt,34,triggerPress,triggerRelease);
      axis(rt,35,triggerPress,triggerRelease);
      const bool fresh=available && (next&~previous)!=0;
      previous=next;
      available=true;
      return fresh;
      }

  private:
    uint64_t previous=0;
    bool available=false;
  };
