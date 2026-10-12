#include "../../common/utils/radialinput.h"

#include <iostream>
#include <stdexcept>

void check(bool ok, const char* message) {
  if(!ok) throw std::runtime_error(message);
  }

int main() {
  try {
    using RadialInput::sector;
    RadialInput::StickSelector sticks;
    check(sticks.update(0,-1,0,0)==std::pair(0.f,0.f),"Opening while running must not select a wheel item");
    check(sticks.update(0,-1,0,0)==std::pair(0.f,0.f),"Held movement remains blocked until centered");
    check(sticks.update(0,-1,1,0)==std::pair(1.f,0.f),"Right stick can select while inherited movement stays blocked");
    sticks.update(0,0,1,0);
    check(sticks.update(-1,0,1,0)==std::pair(-1.f,0.f),"A fresh left-stick tilt takes selection from the right stick");
    check(sticks.update(0,0,1,0)==std::pair(0.f,0.f),"Centering does not switch back to a stale held stick");
    sticks.reset();
    sticks.update(0,0,0,0);
    check(sticks.update(0,-1,0,0)==std::pair(0.f,-1.f),"Left stick selects a newly opened system wheel");
    for(const auto origin : {std::pair{2200.f,160.f},std::pair{2200.f,720.f}}) {
      const auto [x,y]=origin;
      check(RadialInput::touchSector(x,y,x,y,2340,1080,8)==-1,"Holding the original position cancels");
      check(RadialInput::touchSector(x,y-20,x,y,2340,1080,8)==-1,"Small finger jitter stays neutral");
      check(RadialInput::touchSector(x,y-30,x,y,2340,1080,8)==0,"A short upward drag selects the center wheel's top item");
      check(RadialInput::touchSector(x+30,y,x,y,2340,1080,8)==2,"A short rightward drag selects the right item");
      check(RadialInput::touchSector(x,y+30,x,y,2340,1080,2)==1,"A short downward drag selects Journal");
      check(RadialInput::touchSector(x-400,y,x,y,2340,1080,8)==6,"Longer drags retain directional selection beyond the visual wheel");
      check(RadialInput::touchSector(x,y,x,y,2340,1080,2)==-1,"Returning the held finger to its origin cancels");
      }
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
    check(sector(0,-100,50,140,2)==0,"The character wheel selects Stats at the top");
    check(sector(0,-100,50,140,4)==0,"The system wheel selects FPS at the top");
    check(sector(100,0,50,140,4)==1,"The system wheel selects touch debug on the right");
    check(sector(0,100,50,140,4)==2,"The system wheel selects HUD layout at the bottom");
    check(sector(-100,0,50,140,4)==3,"The system wheel selects combat controls on the left");
    check(sector(0,0,50,140,4)==-1,"Releasing without a choice does not toggle a setting");
    for(int count=1;count<=8;++count) {
      for(int i=0;i<count;++i) {
        const float angle=float(i)*2.f*std::numbers::pi_v<float>/float(count);
        check(sector(100*std::sin(angle),-100*std::cos(angle),50,140,count)==i,
              "Adaptive hit testing matches evenly spaced choices");
        check(sector(std::sin(angle),-std::cos(angle),0.5f,2.f,count)==i,
              "Gamepad stick selection matches the same evenly spaced choices");
        }
      check(sector(0,-0.49f,0.5f,2.f,count)==-1,"Gamepad neutral input retains cancellation");
      check(sector(0,0,50,140,count)==-1,"Every wheel size retains center cancellation");
      check(sector(0,141,50,140,count)==-1,"Every wheel size retains outside cancellation");
      }
    for(const auto dimensions : {std::pair{2340,1080},std::pair{1280,720},std::pair{640,360},std::pair{1080,2340}}) {
      for(float scale : {1.f,2.f,4.f}) {
        for(int count : {0,1,2,3,4,8}) {
          const auto [width,height]=dimensions;
          const auto layout=RadialInput::layout(width,height,scale,count,24);
          check(layout.x==float(width)*0.5f && layout.y==float(height)*0.5f,
                "Touch and gamepad wheels always appear at the screen center");
          check(layout.x-layout.outer>=0 && layout.x+layout.outer<=width,
                "The full wheel fits horizontally");
          check(layout.y-layout.outer>=0 && layout.y+layout.outer+layout.footer<=height,
                "The centered wheel leaves room for its labels underneath");
          check(layout.y-layout.outer-24-std::max(4.f,8.f*std::min(scale,float(std::min(width,height))/720.f))>=0,
                "The title fits above the centered wheel");
          check(layout.cell>0 && layout.radius>0,"Visible viewports have usable item dimensions");
          if(count==8)
            check(2*layout.radius*std::sin(std::numbers::pi_v<float>/8.f)>=layout.cell,
                  "Adjacent equipment item viewports do not overlap");
          }
        }
      }
    const auto small=RadialInput::layout(2340,1080,3.f,2,24);
    const auto full=RadialInput::layout(2340,1080,3.f,8,24);
    check(small.outer<full.outer,"Two choices use a smaller wheel than eight choices");
    check(RadialInput::layout(0,0,1,2,24).cell==0,"Hidden viewports have no wheel");
    std::cout << "Radial selection and layout tests passed\n";
    }
  catch(const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
    }
  }
