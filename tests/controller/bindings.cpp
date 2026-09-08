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
    auto overlayHint=[](const B& bindings,A action,C context) {
      for(const auto& hint:bindings.hints(context)) if(hint.action==action) return hint.keys;
      return std::string("None");
      };
    check(overlayHint(b,A::AttackForward,C::ClassicMelee)=="Y","Classic overlay shows the effective attack binding");
    check(overlayHint(b,A::Jump,C::ClassicMelee)=="None","Classic overlay omits Jump when X is assigned to attack left");
    check(overlayHint(b,A::AttackForward,C::ModernMelee)=="RT","Modern overlay uses the current combat context");
    check(overlayHint(b,A::SystemWheel,C::Gameplay)=="Hold:Menu","Overlay keeps hold actions distinct from taps");
    check(overlayHint(b,A::AssignLeft,C::Inventory)=="A+DpadLeft","Inventory overlay includes assignment chords");
    check(overlayHint(b,A::AdjustLeft,C::UI)=="RightStickLeft","Menu overlay includes slider controls");
    for(const auto& hint:b.hints(C::Inventory)) {
      if(hint.action==A::AssignLeft) check(hint.group==B::HintGroup::QuickSlots,"Assignments appear in the quick-slot group");
      if(hint.action==A::Spell3) check(hint.group==B::HintGroup::Spells,"Spell bindings have a separate group");
      if(hint.action==A::Up) check(hint.group==B::HintGroup::Movement,"Menu navigation stays together");
      }
    for(const auto& hint:b.hints(C::ModernMelee)) {
      if(hint.action==A::AttackForward) check(hint.group==B::HintGroup::Actions,"Combat actions stay together");
      if(hint.action==A::QuickSave) check(hint.group==B::HintGroup::Shortcuts,"Quicksave belongs to shortcuts");
      }
    B remappedOverlay;
    std::istringstream overlayConfig("[ModernMelee]\nAttackForward=RB\nBlock=None\n");
    check(remappedOverlay.load(overlayConfig).empty(),"Overlay test remapping loads");
    check(overlayHint(remappedOverlay,A::AttackForward,C::ModernMelee)=="RB" &&
          overlayHint(remappedOverlay,A::DrawSheathe,C::ModernMelee)=="None" &&
          overlayHint(remappedOverlay,A::Block,C::ModernMelee)=="None",
          "Overlay respects remapped, overridden and unbound actions");
    check(b.options.meleeAssist && b.options.meleeAssistMaxAngle==90.f && b.options.meleeAssistMaxDistance==300.f,
          "Missing combat settings use conservative melee assistance defaults");
    check(b.options.meleeFocusRangeScale==0.f,"Mobile melee focus defaults to the scripted monster warning range");
    B combat;
    std::istringstream assist("[Combat]\nMeleeAssist=0\nMeleeAssistMaxAngle=45\nMeleeAssistMaxDistance=200\nMeleeFocusRangeScale=3\n");
    check(combat.load(assist).empty(),"Combat assistance settings validate");
    check(!combat.options.meleeAssist && combat.options.meleeAssistMaxAngle==45.f && combat.options.meleeAssistMaxDistance==200.f,
          "Combat assistance can be disabled and tuned independently of camera settings");
    check(combat.options.meleeFocusRangeScale==3.f,"Melee focus range is independent of facing assistance");
    std::istringstream originalFocus("[Combat]\nMeleeFocusRangeScale=1\n");
    check(combat.load(originalFocus).empty() && combat.options.meleeFocusRangeScale==1.f,
          "Original Gothic focus range can be restored");
    std::istringstream warningFocus("[Combat]\nMeleeFocusRangeScale=0\n");
    check(combat.load(warningFocus).empty() && combat.options.meleeFocusRangeScale==0.f,
          "Automatic warning range can be selected explicitly");
    std::istringstream fractionalFocus("[Combat]\nMeleeFocusRangeScale=0.5\n");
    check(combat.load(fractionalFocus).size()==1 && combat.options.meleeFocusRangeScale==0.f,
          "Scaling below original range is rejected without disabling automatic focus");
    std::istringstream invalidFocus("[Combat]\nMeleeFocusRangeScale=99\n");
    check(combat.load(invalidFocus).size()==1,"Excessive focus scaling is reported");
    std::istringstream invalidAssist("[Combat]\nMeleeAssistMaxAngle=999\nMeleeAssistMaxDistance=-1\n");
    check(combat.load(invalidAssist).size()==2,"Out-of-range combat assistance settings are reported");
    check(b.movementAxis(0.2f,0.f)==std::pair<float,float>(0.f,0.f),"Movement dead zone suppresses small deflections");
    check(b.movementAxis(0.6f,0.f).first<0.4f,"Movement curve softens medium deflections");
    check(b.movementAxis(1.f,0.f)==std::pair<float,float>(1.f,0.f),"Full stick still reaches full movement");
    const auto gentleTouch=b.touchMovementAxis(0.4f,0.f).first;
    const auto firmTouch=b.touchMovementAxis(0.8f,0.f).first;
    check(gentleTouch>0 && gentleTouch<firmTouch && firmTouch<1,"Touch turn speed increases continuously with stick deflection");
    check(b.movementAxis(-1.f,0.f).first==-1.f,"Movement curve preserves direction");
    const auto diagonal=b.movementAxis(1.f,1.f);
    check(diagonal.first*diagonal.first+diagonal.second*diagonal.second<1.001f,"Diagonal movement remains normalized");
    for(float rawX:{-1.f,-0.6f,-0.4f,-0.2f,0.f,0.2f,0.4f,0.6f,1.f,
                    0.6f,0.4f,0.2f,0.f,-0.2f,-0.4f,-0.6f,-1.f}) {
      const auto curved=b.movementAxis(rawX,-0.04f);
      const auto locked=B::targetMovementAxis(curved.first,curved.second);
      check(locked.second==0.f,"Left-right reversal with vertical noise never requests forward movement");
      check(locked.first==curved.first,"Reversal preserves sideways input immediately without a second dead zone");
      check(!b.automaticWalk(rawX,-0.04f,true),"Locked sidestep reversal never enables automatic walk animations");
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
    check(b.automaticWalk(0.4f,0.f,false),"Gentle unlocked movement still walks");
    check(b.automaticWalk(0.f,-0.4f,true),"Gentle locked forward movement still walks");
    check(b.automaticWalk(0.f,0.4f,true),"Gentle locked backward movement retains automatic walk mode");
    check(!b.automaticWalk(0.f,0.f,true),"Neutral locked input does not switch to walk idle");
    check(!b.automaticWalk(0.f,-1.f,true),"Full locked forward input does not walk");
    check(!b.automaticWalk(0.3f,0.2f,true),"Side-dominant diagonal keeps combat sidestep animation");
    for(float x:{-1.f,-0.7f,-0.4f,-0.1f,0.f,0.1f,0.4f,0.7f,1.f})
      check(!b.automaticWalk(x,0.f,true),"Crossing the walk threshold sideways never changes gait");
    std::istringstream defaults(B::defaults());
    check(b.load(defaults).empty(),"Default bindings must validate");
    check(b.touchMovementAxis(0.14f,-0.14f)==std::pair(0.f,0.f),"Touch resting noise remains neutral");
    check(b.touchMovementAxis(0.f,-0.16f).second<0.f,"Touch forward starts just beyond its own dead zone");
    check(b.touchMovementAxis(1.f,0.1f)==std::pair(1.f,0.f),"Full touch turning with vertical noise never walks");
    check(b.touchMovementAxis(0.1f,-1.f)==std::pair(0.f,-1.f),"Full touch movement with horizontal noise never turns");
    check(b.turnMovementAxis(0.9659258f,-0.258819f).first>0.f && b.turnMovementAxis(0.9659258f,-0.258819f).second<0.f,
          "Two thirty steers right while advancing");
    check(b.turnMovementAxis(-0.9659258f,-0.258819f).first<0.f && b.turnMovementAxis(-0.9659258f,-0.258819f).second<0.f,
          "Nine thirty steers left while advancing");
    for(float sign:{-1.f,1.f}) {
      const auto lower=b.turnMovementAxis(sign*0.9659258f,0.258819f);
      check(lower.first*sign>0.f && lower.second==0.f,"Eight thirty and three thirty turn in place");
      const auto upper=b.turnMovementAxis(sign*0.8660254f,-0.5f);
      check(upper.first*sign>0.f && upper.second<0.f,"Upper diagonals move forward while steering");
      }
    check(b.turnMovementAxis(0.f,1.f)==std::pair(0.f,1.f),"Straight down retreats without turning around");
    check(b.turnMovementAxis(0.1f,1.f).first==0.f && b.turnMovementAxis(0.1f,1.f).second>0.f,
          "Backward movement ignores horizontal noise");
    check(b.turnMovementAxis(0.1f,-1.f)==std::pair(0.f,-1.f),"Forward input ignores horizontal noise");
    check(b.turnMovementAxis(0.5f,1.f).second>0.f && b.turnMovementAxis(0.5f,1.f).first>0.f,
          "Backward movement and partial turning can be combined independently");
    check(b.turnMovementAxis(0.f,0.f)==std::pair(0.f,0.f),"Releasing the stick stops turning and movement");
    check(b.touchMovementAxis(1.f,0.f)==std::pair(1.f,0.f),"Touch keeps its horizontal turn-in-place direction");
    check(b.touchMovementAxis(0.8660254f,0.5f).second>0.f,"The gamepad steering shift does not change touch diagonals");
    check(!b.automaticWalk(0.f,-0.65f,false),"Gamepad runs by 65 percent physical stick travel");
    for(float value:{0.63f,0.59f,0.62f,0.57f})
      check(!b.automaticWalk(0.f,-value,false),"Running does not chatter around the walk threshold");
    check(b.automaticWalk(0.f,-0.55f,false),"Backing off deliberately returns to walking");
    for(float value:{0.57f,0.62f,0.59f,0.63f})
      check(b.automaticWalk(0.f,-value,false),"Walking does not chatter around the run threshold");
    check(!b.automaticWalk(0.f,0.f,false),"Neutral immediately releases automatic walking");
    check(b.automaticWalk(0.f,-0.20f,false,true),"Touch can walk before the old forward threshold");
    check(!b.automaticWalk(0.f,-0.40f,false,true),"Touch runs with a short drag");
    check(!b.automaticWalk(0.f,-0.34f,false,true),"Touch keeps running within the gait margin");
    check(b.automaticWalk(0.f,-0.30f,false,true),"Shortening the touch drag deliberately returns to walking");
    b.automaticWalk(0.f,0.f,false);
    check(b.automaticWalk(0.f,-0.40f,false),"Gamepad retains its larger walking region");
    std::istringstream touchWalk("[Axes]\nTouchWalkThreshold=0.5\n");
    check(b.load(touchWalk).empty() && b.options.touchWalkThreshold==0.5f,"Touch walking threshold can be configured independently");
    std::istringstream response("[Axes]\nTouchMovementDeadZone=0.1\nTouchTurnSpeed=240\nWalkHysteresis=0\n");
    check(b.load(response).empty(),"Responsiveness options validate");
    check(b.options.touchMovementDeadZone==0.1f && b.options.touchTurnSpeed==240.f &&
          b.options.walkHysteresis==0.f,"Responsiveness options can be tuned or disabled");
    std::istringstream restoreMovementDefaults(B::defaults());
    check(b.load(restoreMovementDefaults).empty(),"Restore defaults after responsiveness checks");
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
    check(b.hint(A::AdjustLeft,C::UI)=="RightStickLeft" && b.hint(A::AdjustRight,C::UI)=="RightStickRight",
          "Existing INI inherits right-stick value adjustment without replacing left-stick navigation");
    for(const auto& [name,action]:{std::pair("RightStickLeft",A::AdjustLeft),std::pair("RightStickRight",A::AdjustRight)}) {
      const auto direction=B::button(name);
      b.reset();
      auto adjust=b.update(direction,C::UI,0);
      check(has(adjust,action) && !has(adjust,A::Accept) && !has(adjust,A::Back),
            "Right stick requests value adjustment without accepting or closing menus");
      check(b.update(direction,C::UI,100).empty(),"Adjustment waits for the repeat delay");
      check(has(b.update(direction,C::UI,350),action,P::Repeat),"Held right stick repeats value adjustment");
      check(b.update(direction,C::UI,400).empty(),"Value adjustment respects the repeat interval");
      check(has(b.update(direction,C::UI,500),action,P::Repeat),"Value adjustment continues repeating");
      check(has(b.update(0,C::UI,501),action,P::Release),"Centering the stick releases value adjustment");
      check(b.update(0,C::UI,1000).empty(),"Centered stick stops adjusting values");
      b.reset();
      b.update(direction,C::Gameplay,0);
      check(b.update(direction,C::UI,1).empty(),"Opening a menu blocks an already-held camera stick");
      b.update(0,C::UI,2);
      check(has(b.update(direction,C::UI,3),action),"A fresh tilt adjusts after entering the menu");
      check(has(b.update(direction,C::Gameplay,4),action,P::Cancel),"Leaving UI cancels value adjustment");
      check(!has(b.update(direction,C::Gameplay,1000),action,P::Repeat),"Adjustment never repeats in gameplay");
      }
    for(const auto& [name,action]:{std::pair("LeftStickUp",A::Up),std::pair("LeftStickDown",A::Down),
                                 std::pair("LeftStickLeft",A::Left),std::pair("LeftStickRight",A::Right),
                                 std::pair("DpadLeft",A::Left),std::pair("DpadRight",A::Right),
                                 std::pair("A",A::Accept),std::pair("B",A::Back)}) {
      b.reset();
      check(has(b.update(B::button(name),C::UI,0),action),"Menu navigation and dedicated accept/back bindings remain unchanged");
      }
    b.reset();
    check(b.update(B::button("RightStickUp"),C::UI,0).empty(),"Vertical right stick does not navigate menus");
    std::istringstream remapAdjustment("[UI]\nAdjustLeft=LB\nAdjustRight=None\n");
    check(b.load(remapAdjustment).empty(),"Value adjustment can be remapped or disabled");
    b.reset();
    check(has(b.update(B::button("LB"),C::UI,0),A::AdjustLeft),"Remapped value adjustment works");
    b.reset();
    check(b.update(B::button("RightStickRight"),C::UI,0).empty(),"Disabled value adjustment does nothing");
    std::istringstream noDelete("[UI]\nDeleteSave=None\n");
    check(b.load(noDelete).empty(),"Save deletion can be unbound");
    check(!has(b.update(x,C::UI,0),A::DeleteSave),"Unbound save deletion does nothing");
    std::istringstream restoreDefaults(B::defaults());
    check(b.load(restoreDefaults).empty(),"Restore default bindings");
    const auto lb=B::button("LB"), l3=B::button("L3"), up=B::button("DpadUp");
    b.update(lb,C::Gameplay,0);
    auto events=b.update(lb|l3,C::Gameplay,1);
    check(!has(events,A::Walk),"There is no default walk toggle chord");
    b.update(l3,C::Gameplay,2);
    check(!has(b.update(l3,C::Gameplay,1000),A::Sneak),"Modifier release must not leak");
    b.update(0,C::Gameplay,1001);
    check(has(b.update(l3,C::Gameplay,1002),A::Sneak),"Fresh unmodified L3 sneaks");
    b.reset();
    check(b.update(up,C::Gameplay,0).empty(),"Quick slot tap must be deferred");
    check(has(b.update(0,C::Gameplay,100),A::QuickUp),"Short Up uses its assigned item");
    b.update(up,C::Gameplay,200);
    check(has(b.update(up,C::Gameplay,700),A::WheelUp),"Held Up opens its category wheel");
    check(!has(b.update(0,C::Gameplay,701),A::QuickUp),"Wheel release must not trigger a slot tap");
    b.reset();
    b.update(lb,C::Gameplay,0);
    check(has(b.update(lb|up,C::Gameplay,1),A::FirstPerson),"Modified Up bypasses tap/hold");
    check(!has(b.update(lb|up,C::Gameplay,1000),A::WheelUp),"Modified Up never opens wheel");
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
    for(auto context:{C::Gameplay,C::ClassicMelee,C::ModernMelee,C::Ranged}) {
      B system;
      check(system.update(menu,context,0).empty(),"Menu waits to distinguish tap from hold");
      check(has(system.update(0,context,100),A::Pause),"Tapping Menu pauses");
      system.update(menu,context,200);
      check(!has(system.update(menu,context,599),A::SystemWheel),"System wheel respects the hold threshold");
      events=system.update(menu,context,600);
      check(has(events,A::SystemWheel) && !has(events,A::Pause),"Holding Menu opens System without pausing");
      check(!has(system.update(0,context,601),A::Pause),"System wheel release never opens the pause menu");
      system.reset();
      system.update(lb,context,0);
      check(has(system.update(lb|menu,context,1),A::QuickSave),"LB+Menu still saves immediately");
      check(!has(system.update(lb|menu,context,500),A::SystemWheel),"Holding quicksave does not open System");
      system.update(menu,context,501);
      check(!has(system.update(menu,context,1000),A::SystemWheel),"Releasing LB does not turn quicksave into System");
      }
    B system;
    system.update(menu,C::Gameplay,0);
    check(!has(system.update(menu,C::UI,500),A::SystemWheel),"Opening another UI cancels the pending System hold");
    system.reset();
    check(has(system.update(menu,C::UI,0),A::Back),"Menu still backs out immediately inside menus");
    std::istringstream noSystem("[Gameplay]\nSystemWheel=None\n");
    check(system.load(noSystem).empty(),"System wheel can be unbound");
    check(has(system.update(menu,C::Gameplay,0),A::Pause),"Unbinding System restores immediate Menu activation");
    std::istringstream remapSystem("[Gameplay]\nSystemWheel=Hold:View\n");
    check(system.load(remapSystem).empty(),"System wheel can be remapped");
    auto view=B::button("View");
    system.update(view,C::Gameplay,0);
    check(has(system.update(view,C::Gameplay,400),A::SystemWheel),"Remapped System hold opens the wheel");
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
    check(!has(defaultsOnly.update(up,C::UI,500),A::WheelUp),"Context change cancels pending hold");
    defaultsOnly.reset();
    auto lt=B::button("LT"), rt=B::button("RT");
    defaultsOnly.update(lt,C::ModernMelee,0);
    events=defaultsOnly.update(lt|rt,C::ModernMelee,1);
    check(has(events,A::AttackForward) && !has(events,A::Finish),"Modern finisher uses the attack action, not a separate chord");
    defaultsOnly.reset();
    defaultsOnly.update(0,C::ClassicMelee,0);
    check(has(defaultsOnly.update(B::button("Y"),C::ClassicMelee,1),A::AttackForward),"Classic forward attack also owns hold-to-finish");
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
    std::istringstream unbind("[Gameplay]\nQuickUp=None\n");
    check(holdOnly.load(unbind).empty(),"Tap can be unbound independently");
    holdOnly.update(up,C::Gameplay,0);
    check(holdOnly.update(0,C::Gameplay,100).empty(),"A hold-only short tap does nothing");
    std::cout<<"Controller binding tests passed\n";
    }
  catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
  }
