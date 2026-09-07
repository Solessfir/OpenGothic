#include "../../common/utils/radialinput.h"

#include <iostream>
#include <stdexcept>

void check(bool ok, const char* message) {
  if(!ok) throw std::runtime_error(message);
  }

int main() {
  try {
    using RadialInput::sector;
    check(sector(0,0,50,140)==-1,"Releasing in the center cancels");
    check(sector(0,-49,50,140)==-1,"The entire neutral area cancels");
    check(sector(0,-141,50,140)==-1,"Dragging outside the wheel cancels");
    check(sector(0,-100,50,140)==0,"Up selects character stats or equipment slot zero");
    check(sector(100,0,50,140)==2,"Right selects slot two");
    check(sector(0,100,50,140)==4,"Down selects journal or equipment slot four");
    check(sector(-100,0,50,140)==6,"Left reaches the previous-page sector");
    check(sector(-70,-70,50,140)==7,"Upper-left reaches the next-page sector");
    check(sector(-1,-100,50,140)==0,"Selection wraps correctly through the top sector");
    check(sector(1,-100,50,140)==0,"Both sides of the top sector select the same entry");
    check(sector(0,-50,50,140)==0 && sector(0,-140,50,140)==0,"Inner and outer ring boundaries remain selectable");
    for(int i=0;i<8;++i) {
      const float angle=float(i)*std::numbers::pi_v<float>/4.f;
      check(sector(100*std::sin(angle),-100*std::cos(angle),50,140)==i,"Hit testing matches the rendered item positions");
      }
    std::cout << "Touch radial selection tests passed\n";
    }
  catch(const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
    }
  }
