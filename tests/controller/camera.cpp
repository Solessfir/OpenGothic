#include "../../common/utils/cameramath.h"

#include <iostream>
#include <stdexcept>

void check(bool ok, const char* message) {
  if(!ok) throw std::runtime_error(message);
  }

int main() {
  try {
    using namespace CameraMath;
    check(yawDelta(350.f,0.f)==10.f,"350 to 0 takes the short positive turn");
    check(yawDelta(0.f,350.f)==-10.f,"0 to 350 takes the short negative turn");
    check(yawNear(-179.f,179.f)==181.f,"Yaw wraps near the character across positive 180");
    check(yawNear(179.f,-179.f)==-181.f,"Yaw wraps near the character across negative 180");
    check(yawNear(1085.f,0.f)==5.f,"Multiple accumulated turns preserve orientation");
    check(followYawDelta(0,90,0.05f,0.2f,0)==0,"Released movement stick adds no recentering");
    const auto full=followYawDelta(0,90,0.05f,0.2f,1);
    const auto gentle=followYawDelta(0,90,0.05f,0.2f,0.1f);
    check(gentle>0 && gentle<full*0.15f,"Gentle stick input slows recentering");
    check(followYawDelta(350,0,0.05f,0.2f,1)>0,"Lock following crosses zero the short way");
    check(followYawDelta(0,350,0.05f,0.2f,1)<0,"Reverse lock following crosses zero the short way");
    check(followYawDelta(0,10,10,0.2f,1)<=10,"Long frame cannot overshoot target");
    float yaw30=0, yaw120=0;
    for(int i=0;i<30;++i) yaw30+=followYawDelta(yaw30,90,1.f/30.f,0.2f,0.4f);
    for(int i=0;i<120;++i) yaw120+=followYawDelta(yaw120,90,1.f/120.f,0.2f,0.4f);
    check(std::abs(yaw30-yaw120)<0.001f,"Camera assistance is frame-rate independent");
    std::cout << "Camera input tests passed\n";
    }
  catch(const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
    }
  }
