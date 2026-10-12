#include "../../common/utils/meleeassist.h"
#include "../../common/utils/attacktap.h"

#include <iostream>
#include <limits>
#include <stdexcept>

void check(bool ok, const char* message) {
  if(!ok) throw std::runtime_error(message);
  }

int main() {
  try {
    AttackTap tap;
    check(!tap.release(0),"Release without a press cannot swing");
    tap.begin(100,400);
    check(tap.active() && tap.release(150),"An empty-target short press swings on release");
    check(!tap.active() && !tap.release(160),"A tap can be consumed only once");
    tap.begin(200,400);
    check(!tap.release(600),"Reaching the hold threshold cancels the empty-target swing");
    tap.begin(1000,400);
    check(!tap.release(5000),"A long empty-target hold never becomes a release attack");
    tap.begin(6000,400);
    tap.cancel();
    check(!tap.release(6010),"Context cancellation cannot leave a delayed swing");
    tap.begin(7000,400);
    check(!tap.release(6999),"A backwards clock cannot become a tap");
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
