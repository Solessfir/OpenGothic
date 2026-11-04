#include "../../common/utils/stuckwatch.h"

#include <iostream>
#include <stdexcept>

void check(bool ok, const char* message) {
  if(!ok) throw std::runtime_error(message);
  }

int main() {
  try {
    using P = StuckWatch::Pos;
    StuckWatch w;
    check(!w.tick(P{0,100,0},false,true,500),"Falling never counts as stuck");
    bool fired=false;
    for(int t=0;t<7900;t+=100) fired |= w.tick(P{0,0,float(t%40)},true,true,100);
    check(!fired,"Less than the trying time is not stuck");
    check(w.tick(P{0,0,0},true,true,200),"Trying to get out for the whole time is stuck");
    check(!w.tick(P{0,0,0},true,true,100),"After reporting, the time starts over");

    StuckWatch idle;
    bool idleFired=false;
    for(int t=0;t<20000;t+=100) idleFired |= idle.tick(P{0,0,0},true,false,100);
    check(!idleFired,"Standing still without input is never stuck");

    StuckWatch walk;
    bool walkFired=false;
    for(int t=0;t<20000;t+=100) walkFired |= walk.tick(P{float(t)*0.5f,0,0},true,true,100);
    check(!walkFired,"Walking away keeps resetting the circle");
    std::cout << "Stuck watch tests passed" << std::endl;
    }
  catch(const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
    }
  }
