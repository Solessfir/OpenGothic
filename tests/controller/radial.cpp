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
    check(sector(0,-100,50,140,0)==-1,"An empty wheel cannot select anything");
    check(sector(0,100,50,140,2)==1,"The compact character wheel selects Journal at the bottom");
    for(int count=1;count<=8;++count) {
      for(int i=0;i<count;++i) {
        const float angle=float(i)*2.f*std::numbers::pi_v<float>/float(count);
        check(sector(100*std::sin(angle),-100*std::cos(angle),50,140,count)==i,
              "Adaptive hit testing matches evenly spaced choices");
        }
      check(sector(0,0,50,140,count)==-1,"Every wheel size retains center cancellation");
      check(sector(0,141,50,140,count)==-1,"Every wheel size retains outside cancellation");
      }
    for(const auto dimensions : {std::pair{2340,1080},std::pair{1280,720},std::pair{640,360},std::pair{1080,2340}}) {
      for(float scale : {1.f,2.f,4.f}) {
        for(int count : {0,1,2,3,8}) {
          const auto [width,height]=dimensions;
          for(float x : {0.f,float(width)/2.f,float(width)}) {
            for(float y : {0.f,float(height)/2.f,float(height)}) {
              const auto layout=RadialInput::touchLayout(width,height,scale,x,y,count,24);
              check(layout.x-layout.outer>=0 && layout.x+layout.outer<=width,
                    "The full wheel fits horizontally, even when opened at an edge");
              check(layout.y-layout.outer>=0 && layout.y+layout.outer+layout.footer<=height,
                    "The wheel and its labels fit vertically, even when opened at an edge");
              check(layout.cell>0 && layout.radius>0,"Visible viewports have usable item dimensions");
              if(count==8)
                check(2*layout.radius*std::sin(std::numbers::pi_v<float>/8.f)>=layout.cell,
                      "Adjacent equipment item viewports do not overlap");
              }
            }
          }
        }
      }
    const auto small=RadialInput::touchLayout(2340,1080,3.f,2200,900,2,24);
    const auto full=RadialInput::touchLayout(2340,1080,3.f,2200,900,8,24);
    check(small.outer<full.outer,"Two choices use a smaller wheel than eight choices");
    check(RadialInput::touchLayout(0,0,1,0,0,2,24).cell==0,"Hidden viewports have no wheel");
    std::cout << "Touch radial selection tests passed\n";
    }
  catch(const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
    }
  }
