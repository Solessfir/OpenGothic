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
    check(b.movementAxis(0.2f,0.f)==std::pair<float,float>(0.f,0.f),"Movement dead zone suppresses small deflections");
    check(b.movementAxis(0.6f,0.f).first<0.4f,"Movement curve softens medium deflections");
    check(b.movementAxis(1.f,0.f)==std::pair<float,float>(1.f,0.f),"Full stick still reaches full movement");
    check(b.movementAxis(-1.f,0.f).first==-1.f,"Movement curve preserves direction");
    const auto diagonal=b.movementAxis(1.f,1.f);
    check(diagonal.first*diagonal.first+diagonal.second*diagonal.second<1.001f,"Diagonal movement remains normalized");
    for(float rawX:{-1.f,-0.6f,-0.4f,-0.2f,0.f,0.2f,0.4f,0.6f,1.f,
                    0.6f,0.4f,0.2f,0.f,-0.2f,-0.4f,-0.6f,-1.f}) {
      const auto curved=b.movementAxis(rawX,-0.04f);
      const auto locked=B::targetMovementAxis(curved.first,curved.second);
      check(locked.second==0.f,"Left-right reversal with vertical noise never requests forward movement");
      check(locked.first==curved.first,"Reversal preserves sideways input immediately without a second dead zone");
      }
    for(float rawY:{-1.f,-0.4f,0.f,0.4f,1.f}) {
      const auto curved=b.movementAxis(0.04f,rawY);
      const auto locked=B::targetMovementAxis(curved.first,curved.second);
      check(locked.first==0.f,"Forward-back movement ignores small horizontal noise");
      check(locked.second==curved.second,"Intentional forward-back movement remains available while locked");
      }
    check(B::targetMovementAxis(0.f,0.f)==std::pair<float,float>(0.f,0.f),"Neutral locked stick does not retain movement");
    check(B::targetMovementAxis(0.6f,-0.5f)==std::pair<float,float>(0.6f,0.f),"Side-dominant diagonal strafes");
    check(B::targetMovementAxis(0.5f,-0.6f)==std::pair<float,float>(0.f,-0.6f),"Forward-dominant diagonal advances");
    std::istringstream defaults(B::defaults());
    check(b.load(defaults).empty(),"Default bindings must validate");
    const auto x=B::button("X");
    check(has(b.update(x,C::UI,0),A::DeleteSave),"UI X requests save deletion");
    check(!has(b.update(x,C::UI,1000),A::DeleteSave),"Holding X does not repeat deletion");
    b.reset();
    auto drop=b.update(x,C::Inventory,0);
    check(has(drop,A::Drop) && !has(drop,A::DeleteSave),"Inventory X drops without deleting saves");
    b.reset();
    check(!has(b.update(x,C::Gameplay,0),A::DeleteSave),"Gameplay never requests save deletion");
    std::istringstream oldUi("[UI]\nAccept=A\nBack=B,View,Menu\n");
    check(b.load(oldUi).empty(),"Existing UI settings remain valid");
    check(b.hint(A::DeleteSave,C::UI)=="X","Existing INI inherits save deletion binding");
    std::istringstream noDelete("[UI]\nDeleteSave=None\n");
    check(b.load(noDelete).empty(),"Save deletion can be unbound");
    check(!has(b.update(x,C::UI,0),A::DeleteSave),"Unbound save deletion does nothing");
    std::istringstream restoreDefaults(B::defaults());
    check(b.load(restoreDefaults).empty(),"Restore default bindings");
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
    b.reset(a);
    check(b.update(a,C::UI,0).empty(),"Reconnect blocks held input");
    b.update(0,C::UI,1);
    check(has(b.update(a,C::UI,2),A::Accept),"Reconnect permits a fresh press");
    B defaultsOnly;
    defaultsOnly.update(up,C::Gameplay,0);
    check(!has(defaultsOnly.update(up,C::UI,500),A::EquipmentWheel),"Context change cancels pending hold");
    defaultsOnly.reset();
    auto lt=B::button("LT"), rt=B::button("RT");
    defaultsOnly.update(lt,C::ModernMelee,0);
    events=defaultsOnly.update(lt|rt,C::ModernMelee,1);
    check(has(events,A::Finish) && !has(events,A::AttackForward),"Finisher does not fire ordinary attack");
    defaultsOnly.reset();
    defaultsOnly.update(lb,C::Inventory,0);
    check(!has(defaultsOnly.update(lb|menu,C::Inventory,1),A::QuickSave),"UI does not inherit gameplay saves");
    defaultsOnly.reset();
    defaultsOnly.update(lt,C::Inventory,0);
    events=defaultsOnly.update(lt|up,C::Inventory,1);
    check(has(events,A::Spell3) && !has(events,A::Up),"Spell assignment consumes navigation");
    std::istringstream ambiguous("[Gameplay]\nJump=LB+A\nJournal=RB+A\n");
    check(defaultsOnly.load(ambiguous).empty(),"Independent chords validate");
    defaultsOnly.update(lb|rb,C::Gameplay,0);
    events=defaultsOnly.update(lb|rb|a,C::Gameplay,1);
    check(events.empty(),"Ambiguous held modifiers cannot choose an arbitrary action");
    std::istringstream unknown("[Axes]\nDeadZoon=0.2\n[Controller]\nVersion=2\n");
    check(defaultsOnly.load(unknown).size()==2,"Unknown options and versions are reported");
    B holdOnly;
    std::istringstream unbind("[Gameplay]\nDrawSheathe=None\n");
    check(holdOnly.load(unbind).empty(),"Tap can be unbound independently");
    holdOnly.update(up,C::Gameplay,0);
    check(holdOnly.update(0,C::Gameplay,100).empty(),"A hold-only short tap does nothing");
    std::cout<<"Controller binding tests passed\n";
    }
  catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
  }
