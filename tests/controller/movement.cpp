#include "../../common/utils/movementresponse.h"
#include <iostream>
#include <stdexcept>

void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }

int main() {
  try {
    using MovementResponse::turn;
    check(turn(90.f,1.f,180.f,1.f,0.1f)==36.f,"Firm cornering doubles the base turn rate");
    check(turn(90.f,0.1f,180.f,1.f,0.1f)<turn(90.f,1.f,180.f,1.f,0.1f),"Gentle input remains gentler");
    check(turn(90.f,1.f,180.f,0.f,0.1f)==18.f,"Disabling boost restores the base turn limit");
    check(turn(-90.f,1.f,180.f,1.f,0.1f)==-36.f,"Left and right turns are symmetric");
    check(turn(350.f,1.f,180.f,1.f,0.1f)==-10.f,"Wraparound takes the shortest path");
    check(turn(-350.f,1.f,180.f,1.f,0.1f)==10.f,"Opposite wraparound takes the shortest path");
    check(turn(2.f,1.f,180.f,1.f,0.1f)==2.f,"Turning never overshoots the requested heading");
    check(turn(90.f,1.f,180.f,1.f,0.f)==0.f,"Zero time produces no rotation");
    check(turn(90.f,1.f,180.f,1.f,-1.f)==0.f,"Negative time produces no rotation");
    for(int fps:{30,60,120}) {
      float angle=0.f;
      for(int i=0;i<fps;++i)
        angle+=turn(170.f-angle,1.f,180.f,1.f,1.f/float(fps));
      check(std::abs(angle-170.f)<0.001f,"A deliberate reversal completes at different frame rates without overshoot");
      }
    std::cout << "Movement response checks passed\n";
    }
  catch(const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
    }
  }
