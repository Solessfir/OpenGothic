#include "../../common/utils/meleeassist.h"

#include <iostream>
#include <limits>
#include <stdexcept>

void check(bool ok, const char* message) {
  if(!ok) throw std::runtime_error(message);
  }

int main() {
  try {
    using MeleeAssist::facing;
    check(facing(0,45,150,90,300)==45,"An eligible off-center target receives the attack");
    check(facing(0,-45,150,90,300)==-45,"Assistance works on either side");
    check(facing(350,10,150,90,300)==370,"Crossing zero takes the short positive arc");
    check(facing(10,350,150,90,300)==-10,"Crossing zero takes the short negative arc");
    check(!facing(0,120,150,90,300),"Targets outside the assist cone do not turn the player");
    check(!facing(0,180,150,90,300),"Default assistance never snaps to an enemy behind the player");
    check(!facing(0,45,301,90,300),"Distant focused NPCs do not receive assistance");
    check(facing(0,90,300,90,300)==90,"Configured angle and distance boundaries are inclusive");
    check(!facing(0,45,0,90,300),"Coincident targets do not invent a direction");
    check(!facing(0,45,100,0,300),"A zero angle disables assistance");
    check(!facing(0,45,100,90,0),"A zero distance disables assistance");
    check(facing(0,120,150,135,300)==120,"A wider configured angle is respected");
    check(!facing(0,45,150,30,300),"A narrower configured angle is respected");
    check(!facing(0,std::numeric_limits<float>::quiet_NaN(),150,90,300),"Invalid target angles are rejected");
    check(!facing(0,45,std::numeric_limits<float>::infinity(),90,300),"Invalid target distances are rejected");
    std::cout << "Melee assistance tests passed\n";
    }
  catch(const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
    }
  }
