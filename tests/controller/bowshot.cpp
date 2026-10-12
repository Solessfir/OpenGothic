#include "../../common/utils/bufferedshot.h"

#include <iostream>
#include <limits>
#include <stdexcept>

void check(bool ok, const char* message) {
  if(!ok) throw std::runtime_error(message);
  }

int main() {
  try {
    BufferedShot shot;
    shot.release();
    check(!shot.active(),"Release alone cannot request a shot");
    shot.press();
    shot.release();
    shot.advance(16);
    check(shot.active(),"A same-frame tap survives until aiming is ready");
    shot.advance(1200);
    check(shot.active(),"A tap survives the bow raising animation");
    shot.fired();
    check(!shot.active(),"A released tap fires exactly once");

    shot.press();
    shot.fired();
    check(shot.active(),"Holding retains normal repeat fire");
    shot.release();
    check(!shot.active(),"Releasing after a shot does not queue an extra shot");

    shot.press();
    shot.release();
    shot.press();
    shot.release();
    shot.fired();
    check(!shot.active(),"Rapid taps during startup do not build an attack queue");

    shot.press();
    shot.cancel();
    shot.release();
    check(!shot.active(),"Context cancellation cannot leave a delayed shot");
    shot.press();
    shot.release();
    shot.advance(4999);
    check(shot.active(),"Startup can wait up to the animation timeout");
    shot.advance(1);
    check(!shot.active(),"An unfireable request expires");
    shot.press();
    shot.advance(std::numeric_limits<uint64_t>::max());
    check(!shot.active(),"Large time steps cancel without overflow");
    shot.press();
    shot.advance(4000);
    shot.fired();
    shot.advance(4000);
    check(shot.active(),"Successful held shots reset the startup timeout");
    std::cout << "Buffered bow shot tests passed\n";
    }
  catch(const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
    }
  }
