#include "../../common/utils/gamepadbindings.h"
#include "../../common/utils/quickslotinput.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

using B=GamepadBindings;
using A=B::Action;
using C=B::Context;
using P=B::Phase;

void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
bool has(const std::vector<B::Event>& events,A action,P phase=P::Press) {
  for(const auto& event:events) if(event.action==action && event.phase==phase) return true;
  return false;
  }

int main() {
  try {
    const auto a=B::button("A"), lb=B::button("LB"), rb=B::button("RB"), lt=B::button("LT");
    const char* directions[]={"DpadUp","DpadDown","DpadLeft","DpadRight"};
    for(int i=0;i<4;++i) {
      const auto direction=B::button(directions[i]);
      const auto assign=A(int(A::AssignUp)+i), tap=A(int(A::QuickUp)+i), wheel=A(int(A::WheelUp)+i);
      B b;
      check(b.update(a,C::Inventory,0).empty(),"A waits for release before equipping");
      check(b.update(a,C::Inventory,1000).empty(),"Holding A alone does not equip or open a wheel");
      auto events=b.update(a|direction,C::Inventory,1001);
      check(events.size()==1 && has(events,assign),"A plus a direction assigns without navigating or equipping");
      check(b.update(a|direction,C::Inventory,1500).empty(),"Assignment does not repeat");
      events=b.update(direction,C::Inventory,1501);
      check(!has(events,A::Accept),"Releasing A first must not equip");
      check(b.update(direction,C::Inventory,2000).empty(),"Releasing A cannot turn assignment into scrolling");
      b.update(0,C::Inventory,2001);
      check(b.update(a,C::Inventory,2002).empty(),"A starts a fresh tap");
      check(has(b.update(0,C::Inventory,2100),A::Accept),"A without a chord equips on release");
      b.reset();
      b.update(a,C::Inventory,0);
      b.update(a|direction,C::Inventory,1);
      b.update(a,C::Inventory,2);
      check(!has(b.update(0,C::Inventory,3),A::Accept),"Releasing the direction first must not equip either");
      b.reset();
      b.update(a,C::Inventory,0);
      b.update(a,C::UI,1);
      check(!has(b.update(0,C::UI,2),A::Accept),"Closing inventory cancels a pending A tap");
      b.reset();
      const auto nav=A(int(A::Up)+i);
      check(has(b.update(direction,C::Inventory,0),nav),"D-pad alone still navigates immediately");
      check(has(b.update(direction,C::Inventory,350),nav,P::Repeat),"Holding D-pad still scrolls inventory");
      b.reset();
      check(b.update(direction,C::Gameplay,0).empty(),"Gameplay slot waits for tap or hold");
      check(has(b.update(0,C::Gameplay,100),tap),"Tapping each direction uses its own slot");
      b.update(direction,C::Gameplay,200);
      check(has(b.update(direction,C::Gameplay,600),wheel),"Holding each direction opens its own wheel");
      check(!has(b.update(0,C::Gameplay,601),tap),"Wheel release does not also activate a tap");
      }
    for(auto context:{C::Gameplay,C::ClassicMelee,C::ModernMelee,C::Ranged}) {
      B b;
      check(has(b.update(rb,context,0),A::DrawSheathe),"RB draws immediately in every gameplay context");
      check(!has(b.update(0,context,1),A::DrawSheathe),"RB release does not draw again");
      }
    for(auto context:{C::ClassicMelee,C::ModernMelee}) {
      B b;
      check(has(b.update(lt,context,0),A::Block),"LT blocks in both melee modes");
      check(!has(b.update(lt,context,500),A::Block),"Holding LT does not repeat parry");
      }
    B b;
    check(b.options.explorationModifier==lb,"LB is the exploration modifier");
    check(b.hint(A::Walk,C::Gameplay)=="None","Walk is analog, not a default toggle");
    check(has(b.update(rb,C::Inventory,0),A::RightPanel),"RB still switches inventory panels");
    b.reset();
    check(has(b.update(lb,C::Inventory,0),A::LeftPanel),"LB still switches inventory panels");
    std::istringstream remap("[Inventory]\nAssignUp=Y+DpadUp\n");
    check(b.load(remap).empty(),"Slot assignment chords remain remappable");
    const auto y=B::button("Y"), up=B::button("DpadUp");
    check(!has(b.update(y,C::Inventory,0),A::TakeStack),"A remapped modifier must not transfer a stack on press");
    check(has(b.update(y|up,C::Inventory,1),A::AssignUp),"Remapped assignment executes");
    b.update(y,C::Inventory,2);
    check(!has(b.update(0,C::Inventory,3),A::TakeStack),"Remapped assignment consumes the modifier's original action");
    for(std::string_view family:{"ITPO_HEALTH_","ITPO_MANA_","ITFO_POTION_HEALTH_","ITFO_POTION_MANA_"}) {
      for(auto tier:{"01","02","03"})
        check(QuickSlotInput::potionFamily(std::string(family)+tier)==family,"Standard potions preserve their restoration family");
      }
    for(auto name:{"ITPO_PERM_HEALTH","ITPO_HEALTH_PERM","ITPO_MANA_MAX_01","ITFO_POTION_PERM_01","ITPO_HEALTH_99","ITFO_APPLE"})
      check(QuickSlotInput::potionFamily(name).empty(),"Permanent and unknown items must not select automatic substitutes");
    std::cout<<"Quick-slot input tests passed\n";
    }
  catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
  }
