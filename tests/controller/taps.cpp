#include "../../common/utils/multifingertap.h"

#include <iostream>
#include <stdexcept>

void check(bool ok, const char* message) {
  if(!ok) throw std::runtime_error(message);
  }

void fingers(MultiFingerTap& tap, int count) {
  tap.reset();
  for(int i=0;i<count;++i) tap.down(i,float(i*100),100,uint64_t(i*30),true);
  }

int release(MultiFingerTap& tap, int count, uint64_t now=200) {
  for(int i=count-1;i>0;--i) check(tap.up(i,now)==0,"Intermediate finger releases never fire a save or load");
  return tap.up(0,now);
  }

int main() {
  try {
    MultiFingerTap tap;
    using Action=MultiFingerTap::Action;
    check(MultiFingerTap::action(3,true)==Action::DeleteSave,"Three fingers in a save menu only request deletion");
    check(MultiFingerTap::action(4,true)==Action::None,"Four fingers in a save menu never quickload");
    check(MultiFingerTap::action(3,false)==Action::QuickSave,"Gameplay retains three-finger quicksave");
    check(MultiFingerTap::action(4,false)==Action::QuickLoad,"Gameplay retains four-finger quickload");
    check(MultiFingerTap::action(0,true)==Action::None,"Canceled taps cannot request deletion");
    fingers(tap,3);
    check(tap.ready(),"The third stationary finger captures a tap candidate");
    check(release(tap,3)==3,"Exactly three fingers quicksave after all releases");
    fingers(tap,4);
    check(release(tap,4)==4,"Four fingers quickload without first quicksaving");
    fingers(tap,5);
    check(release(tap,5)==0,"Five fingers cannot fall back to save or load");
    fingers(tap,2);
    check(release(tap,2)==0,"Two fingers remain available for swipe gestures");
    fingers(tap,3);
    tap.move(1,120,100,18);
    tap.move(1,100,100,18);
    check(release(tap,3)==0,"Moving out and back still cancels the tap");
    fingers(tap,3);
    check(release(tap,3,801)==0,"Holding too long cancels instead of firing later");
    fingers(tap,3);
    tap.down(3,300,100,351,true);
    check(release(tap,4)==0,"A late fourth finger cancels instead of quicksaving");
    fingers(tap,3);
    check(tap.up(2,100)==0,"The first release does not commit a three-finger tap");
    tap.down(3,300,100,120,true);
    check(tap.up(3,150)==0 && tap.up(1,150)==0 && tap.up(0,150)==0,"Adding fingers after release starts cancels the gesture");
    fingers(tap,2);
    tap.down(2,200,100,60,false);
    check(release(tap,3)==0,"Ineligible modal-UI touches cannot trigger a save");
    tap.reset();
    tap.down(0,1800,500,0,true);
    tap.down(1,300,800,130,true);
    tap.down(2,1200,500,240,true);
    check(tap.ready(),"Spread-out fingers with staggered arrival recognize a tap");
    tap.down(3,2100,750,340,true);
    tap.move(0,1820,520,36);
    check(release(tap,4,700)==4,"A fourth finger arriving later quickloads despite normal fingertip drift");
    tap.reset();
    tap.down(0,1800,200,0,true);
    check(tap.joining(350) && !tap.joining(351),"Button holds wait only through the joining window");
    tap.move(0,1900,200,36);
    check(!tap.joining(50),"A deliberate drag releases the button arbitration delay");
    fingers(tap,3);
    tap.reset();
    check(release(tap,3)==0,"Focus/context cancellation cannot apply on later releases");
    std::cout << "Multi-finger tap tests passed\n";
    }
  catch(const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
    }
  }
