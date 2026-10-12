#include "../../common/utils/twofingerswipe.h"

#include <iostream>
#include <stdexcept>

void check(bool ok, const char* message) {
  if(!ok) throw std::runtime_error(message);
  }

int main() {
  try {
    using namespace TwoFingerSwipe;
    check(canPair(100,5,12),"Nearly simultaneous stationary touches can pair");
    check(canPair(300,5,12),"Staggered finger placement can still start a swipe");
    check(!canPair(351,0,12),"An extra finger on an established stick is not a gesture");
    check(!canPair(100,13,12),"A moving first finger cannot become a two-finger gesture");
    check(direction(0,-60,0,-60,60,250)==-1,"Two upward fingers recognize up");
    check(direction(0,60,0,60,60,250)==1,"Two downward fingers recognize down");
    check(direction(10,80,-15,65,60,300)==1,"Small independent sideways drift is allowed");
    check(direction(0,-80,0,0,60,300)==0,"One moving finger is not a swipe");
    check(direction(0,-80,0,80,60,300)==0,"A vertical pinch is not a swipe");
    check(direction(80,60,80,60,60,300)==0,"Mostly horizontal drags are ignored");
    check(direction(0,59,0,59,60,300)==0,"Jitter below the travel threshold is ignored");
    check(direction(0,80,0,80,60,701)==0,"A slow two-finger hold cannot fire a late swipe");
    check(horizontalDirection(-80,10,-65,-10,60,300)==-1,"Two leftward fingers select health despite small vertical drift");
    check(horizontalDirection(80,10,65,-10,60,300)==1,"Two rightward fingers select mana");
    check(horizontalDirection(-80,0,80,0,60,300)==0,"A horizontal pinch cannot consume a potion");
    check(horizontalDirection(80,0,0,0,60,300)==0,"Moving only one finger cannot consume a potion");
    check(horizontalDirection(60,80,60,80,60,300)==0,"Mostly vertical input cannot consume a potion");
    std::cout << "Two-finger swipe recognition tests passed\n";
    }
  catch(const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
    }
  }
