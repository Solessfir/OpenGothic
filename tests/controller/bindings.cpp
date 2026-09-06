#include "../../common/utils/gamepadbindings.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

using B=GamepadBindings;
using A=B::Action;
using P=B::Phase;
using C=B::Context;
void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
bool has(const std::vector<B::Event>& events,A action,P phase=P::Press) {
  for(auto& e:events) if(e.action==action && e.phase==phase) return true;
  return false;
  }
int main() {
  try {
    B b;
    std::istringstream defaults(B::defaults());
    check(b.load(defaults).empty(),"Default bindings must validate");
    const auto lb=B::button("LB"), l3=B::button("L3"), up=B::button("DpadUp");
    b.update(lb,C::Gameplay,0);
    auto events=b.update(lb|l3,C::Gameplay,1);
    check(has(events,A::Walk) && !has(events,A::Sneak),"Walk chord must not sneak");
    b.update(l3,C::Gameplay,2);
    check(!has(b.update(l3,C::Gameplay,1000),A::Sneak),"Modifier release must not leak");
    b.update(0,C::Gameplay,1001);
    check(has(b.update(l3,C::Gameplay,1002),A::Sneak),"Fresh unmodified L3 sneaks");
    b.reset();
    check(b.update(up,C::Gameplay,0).empty(),"Draw tap must be deferred");
    check(has(b.update(0,C::Gameplay,100),A::DrawSheathe),"Short Up draws");
    b.update(up,C::Gameplay,200);
    check(has(b.update(up,C::Gameplay,700),A::EquipmentWheel),"Held Up opens wheel");
    check(!has(b.update(0,C::Gameplay,701),A::DrawSheathe),"Wheel release must not draw");
    b.reset();
    b.update(lb,C::Gameplay,0);
    check(has(b.update(lb|up,C::Gameplay,1),A::FirstPerson),"Modified Up bypasses tap/hold");
    check(!has(b.update(lb|up,C::Gameplay,1000),A::EquipmentWheel),"Modified Up never opens wheel");
    b.reset();
    auto a=B::button("A");
    check(has(b.update(a,C::ClassicMelee,0),A::Block),"Classic A blocks");
    events=b.update(a,C::UI,1);
    check(has(events,A::Block,P::Cancel) && !has(events,A::Accept),"Entering UI cancels without accepting");
    b.update(0,C::UI,2);
    check(has(b.update(a,C::UI,3),A::Accept),"Fresh UI A accepts");
    b.reset();
    b.update(up,C::UI,0);
    check(has(b.update(up,C::UI,400),A::Up,P::Repeat),"UI repeat works");
    std::istringstream remap("[Gameplay]\nWalk=RB+L3\nSneak=None\n");
    check(b.load(remap).empty(),"Partial remapping validates");
    auto rb=B::button("RB");
    b.update(rb,C::Gameplay,0);
    check(has(b.update(rb|l3,C::Gameplay,1),A::Walk),"Remapped walk works");
    std::istringstream invalid("[Gameplay]\nWalk=Banana\n[Axes]\nTriggerPressThreshold=0.2\nTriggerReleaseThreshold=0.8\n");
    check(b.load(invalid).size()==2,"Invalid inputs report diagnostics");
    check(b.hint(A::Walk,C::Gameplay)=="RB+L3","Invalid section preserves last valid bindings");
    check(b.options.triggerRelease<b.options.triggerPress,"Invalid thresholds restore valid defaults");
    b.reset();
    auto menu=B::button("Menu");
    b.update(lb,C::Gameplay,0);
    events=b.update(lb|menu,C::Gameplay,1);
    check(has(events,A::QuickSave) && !has(events,A::Pause),"Quicksave consumes Menu");
    check(!has(b.update(menu,C::Gameplay,2),A::Pause),"No Menu fallthrough on LB release");
    std::cout<<"Controller binding tests passed\n";
    }
  catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
  }
