#include "mainwindow.h"
#include "gothic.h"
#include "utils/cameramath.h"
#include "world/objects/npc.h"
#include "world/objects/interactive.h"
#include <Tempest/Application>
#include <cmath>

using namespace Tempest;
using PadAction=GamepadBindings::Action;
using Context=GamepadBindings::Context;
using Phase=GamepadBindings::Phase;

Context MainWindow::controllerContext() const {
  if(video.isActive() || rootMenu.isActive() || chapter.isActive() || document.isActive() || console.isActive()) return Context::UI;
  if(inventory.isWheelOpen()) return Context::EquipmentWheel;
  if(inventory.isOpen()==InventoryMenu::State::LockPicking) return Context::Interaction;
  if(inventory.isActive()) return Context::Inventory;
  if(dialogs.isActive()) return Context::UI;
  auto pl=Gothic::inst().player();
  if(pl==nullptr) return Context::UI;
  if(pl->interactive()!=nullptr) return Context::Interaction;
  if(pl->isSwim() || pl->isDive()) return Context::Gameplay;
  const auto ws=pl->weaponState();
  if(ws==WeaponState::Bow || ws==WeaponState::CBow || ws==WeaponState::Mage) return Context::Ranged;
  if(ws==WeaponState::NoWeapon) return Context::Gameplay;
#if defined(__ANDROID__)
  if(player.isClassicCombat() && !controllerExploration) return Context::ClassicMelee;
#endif
  return Context::ModernMelee;
  }

void MainWindow::controllerUiKey(Event::KeyType key,bool repeat) {
  Widget* target=nullptr;
  if(video.isActive()) target=&video;
  else if(rootMenu.isActive()) target=&rootMenu;
  else if(chapter.isActive()) target=&chapter;
  else if(document.isActive()) target=&document;
  else if(console.isActive()) target=&console;
  else if(dialogs.isActive()) target=&dialogs;
  if(target==nullptr) return;
  KeyEvent event(key,Event::M_NoModifier,repeat?Event::KeyRepeat:Event::KeyDown);
  // Dispatch only to the active layer. Ignored UI input must never reach gameplay.
  if(target==&rootMenu) {
    if(repeat) rootMenu.keyRepeatEvent(event); else rootMenu.keyDownEvent(event);
    }
  else if(target==&video) { if(!repeat) video.keyDownEvent(event); }
  else if(target==&chapter) { if(!repeat) chapter.keyDownEvent(event); }
  else if(target==&document) { if(!repeat) document.keyDownEvent(event); }
  else if(target==&dialogs) dialogs.keyDownEvent(event);
  else if(target==&console && key==Event::K_ESCAPE) console.close();
  // Console text input remains owned by the keyboard/IME.
  }

void MainWindow::controllerAction(const GamepadBindings::Event& event) {
#if defined(__ANDROID__)
  const auto action=event.action;
  const bool pressed=event.phase==Phase::Press;
  const bool repeat=event.phase==Phase::Repeat;
  const bool released=event.phase==Phase::Release || event.phase==Phase::Cancel;
  const auto context=controllerContext();
  if(released) {
    const bool cancel=event.phase==Phase::Cancel;
    if(action==PadAction::AttackForward) player.controllerCombat(0,false,cancel);
    if(action==PadAction::AttackLeft) player.controllerCombat(2,false,cancel);
    if(action==PadAction::AttackRight) player.controllerCombat(3,false,cancel);
    if(action==PadAction::Block) player.controllerCombat(1,false,cancel);
    if(action==PadAction::LookBehind) player.releaseControllerKey(KeyCodec::LookBack,cancel);
    if(action==PadAction::Jump) player.releaseControllerKey(KeyCodec::Jump,cancel);
    if(action==PadAction::Up) player.releaseControllerKey(KeyCodec::Forward,cancel);
    if(action==PadAction::Down || action==PadAction::Back) player.releaseControllerKey(KeyCodec::Back,cancel);
    if(action==PadAction::Accept) player.releaseControllerKey(KeyCodec::ActionGeneric,cancel);
    return;
    }
  if(!pressed && !repeat) return;
  if(context==Context::EquipmentWheel) {
    if(action==PadAction::PreviousPage) inventory.wheelPage(-1);
    if(action==PadAction::NextPage) inventory.wheelPage(1);
    if(action==PadAction::Cancel) { inventory.close(); wheelHeldMask=0; player.clearInput(); }
    return;
    }
  if(context==Context::Inventory) { inventory.controllerAction(int(action)); return; }
  if(context==Context::UI) {
    if(action==PadAction::DeleteSave && pressed && rootMenu.isActive() && !video.isActive()) {
      rootMenu.requestDeleteSave(controllerBindings.hint(PadAction::Accept,context)+": delete    "+
                                 controllerBindings.hint(PadAction::Back,context)+": cancel");
      return;
      }
    Event::KeyType key=Event::K_NoKey;
    if(action==PadAction::Accept) key=Event::K_Return;
    if(action==PadAction::Back) key=Event::K_ESCAPE;
    if(action==PadAction::Up) key=Event::K_Up;
    if(action==PadAction::Down) key=Event::K_Down;
    if(action==PadAction::Left) key=Event::K_Left;
    if(action==PadAction::Right) key=Event::K_Right;
    if(key!=Event::K_NoKey) controllerUiKey(key,repeat);
    return;
    }
  auto& gothic=Gothic::inst();
  auto pl=gothic.player();
  auto world=gothic.world();
  auto camera=gothic.camera();
  if(pl==nullptr || world==nullptr || gothic.isPause() || world->isCutsceneLock() || (camera && camera->isCutscene())) return;
  auto key=[&](KeyCodec::Action a) { player.onKeyPressed(a,Event::K_NoKey,KeyCodec::Mapping::Secondary); };
  if(context==Context::Interaction) {
    if(action==PadAction::Up) key(KeyCodec::Forward);
    if(action==PadAction::Down || action==PadAction::Back) key(KeyCodec::Back);
    if(action==PadAction::Left) { key(KeyCodec::Left); player.releaseControllerKey(KeyCodec::Left); }
    if(action==PadAction::Right) { key(KeyCodec::Right); player.releaseControllerKey(KeyCodec::Right); }
    if(action==PadAction::Accept) key(KeyCodec::ActionGeneric);
    return;
    }
  if(repeat) return;
  switch(action) {
    case PadAction::Interact: player.controllerInteract(controllerExploration); break;
    case PadAction::Back:
      if(pl->interactive()!=nullptr) key(KeyCodec::Back);
      break;
    case PadAction::Jump: key(KeyCodec::Jump); break;
    case PadAction::Walk: key(KeyCodec::Walk); break;
    case PadAction::Sneak: key(KeyCodec::Sneak); break;
    case PadAction::FirstPerson: key(KeyCodec::FirstPerson); break;
    case PadAction::LookBehind: key(KeyCodec::LookBack); break;
    case PadAction::DrawSheathe: key(KeyCodec::Weapon); break;
    case PadAction::LockTarget: key(KeyCodec::LockTarget); break;
    case PadAction::AttackForward: player.controllerCombat(0,true,false,controllerBindings.options.holdMs); break;
    case PadAction::AttackLeft: player.controllerCombat(2,true); break;
    case PadAction::AttackRight: player.controllerCombat(3,true); break;
    case PadAction::Block: player.controllerCombat(1,true); break;
    case PadAction::Finish: player.controllerCombat(6,true); break;
    case PadAction::HealthPotion: world->script().playerHotLameHeal(*pl); break;
    case PadAction::ManaPotion: world->script().playerHotLamePotion(*pl); break;
    case PadAction::Map: world->script().playerHotKeyScreenMap(*pl); break;
    case PadAction::QuickSave:
      if(gothic.isInGameAndAlive() && Gothic::settingsGetI("GAME","useQuickSaveKeys")) gothic.quickSave();
      break;
    case PadAction::QuickLoad:
      if(Gothic::settingsGetI("GAME","useQuickSaveKeys")) gothic.quickLoad();
      break;
    case PadAction::Inventory: player.clearInput(); inventory.open(*pl); break;
    case PadAction::EquipmentWheel: {
      player.clearInput();
      inventory.openWheel(*pl);
      if(inventory.isWheelOpen()) {
        wheelHeldMask=event.mask;
        const auto& b=controllerBindings;
        inventory.setWheelHint(b.hint(PadAction::EquipmentWheel,context)+": release to equip   "+
          b.hint(PadAction::PreviousPage,Context::EquipmentWheel)+" / "+b.hint(PadAction::NextPage,Context::EquipmentWheel)+
          ": pages   "+b.hint(PadAction::Cancel,Context::EquipmentWheel)+": cancel");
        }
      break;
      }
    case PadAction::Pause:
    case PadAction::CharacterStats:
    case PadAction::Journal: {
      const auto act=action==PadAction::Pause?KeyCodec::Escape:(action==PadAction::Journal?KeyCodec::Log:KeyCodec::Status);
      rootMenu.setMenu(action==PadAction::Pause?gothic.menuMain():(action==PadAction::Journal?"MENU_LOG":"MENU_STATUS"),act);
      rootMenu.showVersion(action==PadAction::Pause);
      rootMenu.setPlayer(*pl);
      player.clearInput();
      break;
      }
    default: break;
    }
#else
  (void)event;
#endif
  }

void MainWindow::tickGamepad() {
#if defined(__ANDROID__)
  const auto now=Application::tickCount();
  const auto dt=std::min<uint64_t>(50,now-controllerLastPoll);
  controllerLastPoll=now;
  const auto gp=SystemApi::gamepadState();
  auto& options=controllerBindings.options;
  // Track physical presence separately from focus so background disconnections are not lost.
  if(controllerWasPresent && !gp.connected)
    controllerDisconnectPending = Gothic::inst().isInGame() || Gothic::inst().checkLoading()!=Gothic::LoadState::Idle;
  controllerWasPresent = gp.connected && options.enabled;
  const bool connected=gp.connected && options.enabled && controllerFocused;
  mobileUi.setTouchEnabled(!connected && controllerFocused);
  if(touchWheelOwned && (!inventory.isWheelOpen() || Gothic::inst().isPause() ||
     Gothic::inst().checkLoading()!=Gothic::LoadState::Idle || rootMenu.isActive() ||
     dialogs.isActive() || video.isActive() || chapter.isActive() || document.isActive() || console.isActive()))
    mobileUi.cancelWheel();
  const auto touchPlayer = Gothic::inst().player();
  const auto touchWeapon = touchPlayer!=nullptr ? touchPlayer->weaponState() : WeaponState::NoWeapon;
  mobileUi.setDebugContext(Gothic::inst().version().game!=2 || Gothic::settingsGetI("GAME","useGothic1Controls")!=0,
                           video.isActive() || rootMenu.isActive() || chapter.isActive() ||
                           document.isActive() || dialogs.isActive() || inventory.isActive() || console.isActive(),
                           touchPlayer!=nullptr && touchPlayer->weaponState()!=WeaponState::NoWeapon &&
                           !Gothic::inst().isPause() && Gothic::inst().checkLoading()==Gothic::LoadState::Idle,
                           player.lockedTarget()!=nullptr,
                           (touchWeapon==WeaponState::Fist || touchWeapon==WeaponState::W1H || touchWeapon==WeaponState::W2H) &&
                           !Gothic::inst().isPause() && Gothic::inst().checkLoading()==Gothic::LoadState::Idle);
  mobileUi.tick();
  if(!connected && controllerConnected) {
    controllerAxesBlocked=true;
    player.clearInput(); controllerBindings.reset(); controllerButtons=0; controllerTriggers=0;
    if(inventory.isWheelOpen()) inventory.close();
    wheelHeldMask=0;
    }
  controllerConnected=connected;
  if(controllerDisconnectPending && controllerFocused && Gothic::inst().checkLoading()==Gothic::LoadState::Idle) {
    controllerDisconnectPending = false;
    if(auto pl = Gothic::inst().player(); pl!=nullptr && !rootMenu.isActive()) {
      rootMenu.setMenu(Gothic::inst().menuMain(),KeyCodec::Escape);
      rootMenu.showVersion(true);
      rootMenu.setPlayer(*pl);
      player.clearInput();
      controllerBindings.reset(controllerButtons);
      controllerAxesBlocked = true;
      }
    }
  auto camera=Gothic::inst().camera();
  if(!connected) {
    const auto touchMove=mobileUi.movementAxis();
    const auto look=mobileUi.takeLookDelta();
    const bool touchUiActive = video.isActive() || rootMenu.isActive() || chapter.isActive() || document.isActive() ||
                               console.isActive() || dialogs.isActive() || inventory.isActive();
    if(touchUiActive || !controllerFocused || Gothic::inst().isPause() || camera==nullptr || camera->isCutscene() ||
       Gothic::inst().checkLoading()!=Gothic::LoadState::Idle) {
      mobileUi.setAnalogMovement(false);
      if(!touchMovementBlocked)
        player.clearInput();
      touchMovementBlocked = true;
      player.setGamepadAxis(0.f,0.f);
      touchLookIdle = 0;
      return;
      }
    if(touchMove==PointF())
      touchMovementBlocked = false;
    const bool swimming = touchPlayer!=nullptr && (touchPlayer->isSwim() || touchPlayer->isDive());
    const bool locked = !swimming && player.lockedTarget()!=nullptr;
    const bool classicAction = player.isClassicCombat() && player.isPressed(KeyCodec::ActionGeneric);
    const bool targetMovement = locked && !classicAction;
    mobileUi.setAnalogMovement(targetMovement || swimming);
    if(targetMovement || swimming) {
      // Remove keyboard-style turning without clearing target lock or held combat actions.
      player.clearMovementInput();
      const auto axis = touchMovementBlocked ? std::pair(0.f,0.f) : controllerBindings.movementAxis(touchMove.x,touchMove.y);
      if(swimming)
        player.setControllerSwim(axis.first,axis.second,camera->spin().y,camera->spin().x,options.movementTurnSpeed);
      else
        player.setControllerMovement(axis.first,axis.second,camera->spin().y,
                                     controllerBindings.automaticWalk(axis.first,axis.second,true),options.movementTurnSpeed);
      }
    else {
      player.setGamepadAxis(0.f,touchMovementBlocked || (locked && classicAction) ? 0.f : touchMove.y);
      }
    const float dtSec=float(dt)/1000.f;
    float yaw=float(look.x)*300.f/float(std::max(w(),1));
    float pitch=float(look.y)*220.f/float(std::max(h(),1));
    const float sensitivity=Gothic::settingsGetF("GAME","mouseSensitivity")/0.5f;
    yaw*=sensitivity; pitch*=sensitivity;
    if(Gothic::settingsGetI("GAME","camLookaroundInverse")) pitch=-pitch;
    if(mobileUi.isLooking() || look!=Point()) touchLookIdle=0;
    else touchLookIdle+=dt;
    camera->onRotateMouse(PointF(pitch,locked ? 0.f : -yaw));
    const float movement = touchMovementBlocked ? 0.f : std::max(std::abs(touchMove.x),std::abs(touchMove.y));
    if(auto pl = Gothic::inst().player(); pl!=nullptr && !swimming && (locked || (touchLookIdle>800 && movement>0.35f))) {
      const float follow = CameraMath::followYawDelta(camera->spin().y,pl->rotation(),dtSec,options.cameraSmoothing,
                                                    locked ? 1.f : movement);
      camera->onRotateMouse(PointF(0.f,follow));
      }
    return;
    }
  auto trigger=[&](float value,uint32_t bit) {
    if(value>=((controllerTriggers&bit)?options.triggerRelease:options.triggerPress)) controllerTriggers|=bit;
    else controllerTriggers&=~bit;
    };
  uint32_t axes=0;
  auto directions=[&](float x,float y,int shift) {
    if(y< -0.55f) axes|=1u<<shift;
    if(y> 0.55f) axes|=1u<<(shift+1);
    if(x< -0.55f) axes|=1u<<(shift+2);
    if(x> 0.55f) axes|=1u<<(shift+3);
    };
  auto dispatch=[&](const auto& sample) {
    trigger(sample.leftTrigger,1u<<14); trigger(sample.rightTrigger,1u<<15);
    axes=0;
    directions(sample.leftStickX,sample.leftStickY,16); directions(sample.rightStickX,sample.rightStickY,20);
    controllerButtons=sample.buttons|controllerTriggers|axes;
    controllerExploration=(controllerButtons&options.explorationModifier)!=0;
    auto context=controllerContext();
    if(gp.overflow || Gothic::inst().checkLoading()!=Gothic::LoadState::Idle) {
      controllerAxesBlocked=true;
      controllerBindings.reset(controllerButtons); player.clearInput(); return;
      }
    if(inventory.isWheelOpen() && (controllerButtons&wheelHeldMask)!=wheelHeldMask) {
      const auto selected=inventory.wheelSelection();
      inventory.close(); wheelHeldMask=0;
      player.clearInput(); controllerBindings.reset(controllerButtons);
      if(selected!=size_t(-1)) player.controllerEquip(selected);
      return;
      }
    auto events=controllerBindings.update(controllerButtons,context,now);
    for(auto& event:events) {
      controllerAction(event);
      if(controllerContext()!=context) { controllerAxesBlocked=true; break; }
      }
    };
  if(!gp.overflow) for(auto& sample:gp.changes) dispatch(sample);
  dispatch(gp);

  auto deadZone=[&](float x,float y) {
    const float magnitude=std::sqrt(x*x+y*y);
    if(magnitude<=options.deadZone) return PointF();
    const float scale=(std::min(magnitude,1.f)-options.deadZone)/(1.f-options.deadZone)/magnitude;
    return PointF(x*scale,y*scale);
    };
  const auto left=deadZone(gp.leftStickX,gp.leftStickY), right=deadZone(gp.rightStickX,gp.rightStickY);
  if(inventory.isWheelOpen()) {
    const auto stick=options.swapWheel?left:right;
    inventory.wheelMove(stick.x,stick.y);
    }
  const auto context=controllerContext();
  if(context==Context::UI || context==Context::Inventory || context==Context::EquipmentWheel || context==Context::Interaction ||
     Gothic::inst().isPause() || camera==nullptr || camera->isCutscene()) {
    controllerAxesBlocked=true;
    player.setGamepadAxis(0,0);
    return;
    }
  if(controllerAxesBlocked) {
    if(left!=PointF() || right!=PointF()) {
      player.setGamepadAxis(0,0);
      return;
      }
    controllerAxesBlocked=false;
    }
  const auto movement=options.swapMovement ? controllerBindings.movementAxis(gp.rightStickX,gp.rightStickY) :
                                           controllerBindings.movementAxis(gp.leftStickX,gp.leftStickY);
  const PointF move(movement.first,movement.second);
  auto look=options.swapCamera?left:right;
  const float magnitude=std::sqrt(move.x*move.x+move.y*move.y);
  auto pl=Gothic::inst().player();
  const bool swimming=pl!=nullptr && (pl->isSwim() || pl->isDive());
  const bool lockedGround=player.lockedTarget()!=nullptr && !swimming;
  const bool automaticWalk=controllerBindings.automaticWalk(move.x,move.y,lockedGround);
  if(swimming)
    player.setControllerSwim(move.x,move.y,camera->spin().y,camera->spin().x,options.movementTurnSpeed);
  else
    player.setControllerMovement(move.x,move.y,camera->spin().y,automaticWalk,options.movementTurnSpeed);
  const float dtSec=float(dt)/1000.f;
  const float sensitivity=Gothic::settingsGetF("GAME","mouseSensitivity")/0.5f;
  const float inverse=Gothic::settingsGetI("GAME","camLookaroundInverse")?-1.f:1.f;
  const bool locked=player.lockedTarget()!=nullptr;
  // Lock owns horizontal tracking, but vertical look remains under player control.
  camera->onRotateMouse(PointF(look.y*140.f*dtSec*sensitivity*inverse,locked?0.f:-look.x*180.f*dtSec*sensitivity));
  if(locked) {
    if(std::abs(look.x)<options.switchReset) controllerSwitchReady=true;
    if(controllerSwitchReady && std::abs(look.x)>options.switchThreshold && now-controllerLastSwitch>=options.switchCooldownMs) {
      player.switchControllerTarget(look.x>0); controllerSwitchReady=false; controllerLastSwitch=now;
      }
    }
  if(look!=PointF()) controllerLookIdle=0; else controllerLookIdle+=dt;
  if(pl && !swimming && !player.isPressed(KeyCodec::LookBack) && !camera->isFirstPerson() &&
     (player.lockedTarget()!=nullptr || (options.cameraAssist && magnitude>0 && controllerLookIdle>800))) {
    // Gentle movement should produce gentle camera assistance, without disabling stationary lock tracking.
    const float strength=player.lockedTarget()!=nullptr?1.f:magnitude;
    const float yaw=CameraMath::followYawDelta(camera->spin().y,pl->rotation(),dtSec,options.cameraSmoothing,strength);
    camera->onRotateMouse(PointF(0,yaw));
    }
#endif
  }
