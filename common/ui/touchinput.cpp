#include "touchinput.h"

#include <Tempest/Painter>
#include <Tempest/Application>

#include "gothic.h"
#include "resources.h"
#include "utils/gthfont.h"
#include "utils/twofingerswipe.h"

#include <algorithm>
#include <cmath>

using namespace Tempest;

TouchInput::TouchInput(CommandHandler command, WheelHandler wheel)
  :command(std::move(command)),wheel(std::move(wheel)) {
  }

void TouchInput::paintEvent(Tempest::PaintEvent& e) {
  if(!debugOverlay || wheelPointer>=0)
    return;

  Painter p(e);
  if(blockVisible())
    drawBlock(p);
  const auto gold = Color(0.843f,0.761f,0.631f,0.75f);
  const auto active = Color(1.f,0.85f,0.4f,0.95f);
  const float scale = std::min(Gothic::interfaceScale(this),float(h())/480.f);
  const auto& font = Resources::font(scale);
  const int pad = std::max(8,int(12*scale));
  const int line = font.pixelSize();
  const int moveEnd = w()/2;
  const int lookEnd = (w()*LookBoundaryPercent)/100;
  const auto actionRect = buttonRect(4);
  p.setPen(Pen(gold,Painter::Alpha,2.f));
  p.setBrush(gold);
  p.drawLine(moveEnd,0,moveEnd,h());
  p.drawLine(lookEnd,0,lookEnd,actionRect.y);
  p.drawLine(actionRect.x,actionRect.y,lookEnd,actionRect.y);
  p.drawLine(actionRect.x,actionRect.y,actionRect.x,h());

  auto label = [&](int x,int y,int width,std::string_view text) {
    font.drawText(p,x,y,width,3*line,text,AlignHCenter);
    };
  label(pad,2*line,moveEnd-2*pad,uiActive ? "MENU DIRECTIONS" : "MOVE / TURN");
  label(moveEnd+pad,2*line,lookEnd-moveEnd-2*pad,"CAMERA DRAG");
  label(pad,4*line,lookEnd-2*pad,!touchEnabled ? "Gamepad active: virtual touch zones inactive" :
        (uiActive ? "Touch debug: menu" : (classicCombat ? "Touch debug: Gothic 1 controls" : "Touch debug: Gothic 2 controls")));
  if(!touchEnabled)
    label(pad,6*line,lookEnd-2*pad,"Gameplay touches ignored; Android keyboard still available");
  else if(!uiActive && classicCombat) {
    label(pad,6*line,lookEnd-2*pad,"Hold ACTION, then move stick: up = attack, down = block");
    label(pad,7*line,lookEnd-2*pad,"Left / right = side attacks with melee weapon drawn");
    }
  if(touchEnabled && !uiActive && gesturesEnabled) {
    label(pad,h()-3*line,moveEnd-2*pad,"2 fingers: up = first person; down + hold = look behind");
    label(moveEnd+pad,h()/2,lookEnd-moveEnd-2*pad,"2 fingers: up = stand; down = sneak");
    label(pad,h()-5*line,moveEnd-2*pad,"3-finger tap: quicksave; 4-finger tap: quickload");
    }

  const char* names[] = {"BACK / MENU","INVENTORY","JUMP","DRAW / SHEATHE",
                        uiActive ? "ACCEPT" : (classicCombat ? "HOLD ACTION" : "USE / ATTACK")};
  for(int i=0;i<5;++i) {
    const auto rect = buttonRect(size_t(i));
    bool pressed = false;
    for(const auto& [id,touch]:touches)
      pressed |= touch.role==Role::Button && touch.command==Buttons[i];
    p.setBrush(pressed ? Color(0.8f,0.6f,0.2f,0.32f) : Color(0.04f,0.03f,0.02f,0.18f));
    p.drawRect(rect);
    p.setBrush(gold);
    p.drawLine(rect.x,rect.y,w(),rect.y);
    label(rect.x+pad,rect.y+rect.h/2-line,rect.w-2*pad,names[i]);
    if(i==4 && canLock && touchEnabled && !uiActive)
      label(rect.x+pad,rect.y+rect.h/2+line,rect.w-2*pad,targetLocked ? "DRAG: UNLOCK" : "DRAG: LOCK");
    if((i==0 || i==3) && touchEnabled && !uiActive)
      label(rect.x+pad,rect.y+rect.h/2+line,rect.w-2*pad,i==0 ? "HOLD: CHARACTER" : "HOLD: EQUIPMENT");
    }

  auto cross = [&](Point pos,int radius) {
    p.drawLine(pos.x-radius,pos.y,pos.x+radius,pos.y);
    p.drawLine(pos.x,pos.y-radius,pos.x,pos.y+radius);
    };
  auto box = [&](Point pos,int radius) {
    p.drawLine(pos.x-radius,pos.y-radius,pos.x+radius,pos.y-radius);
    p.drawLine(pos.x+radius,pos.y-radius,pos.x+radius,pos.y+radius);
    p.drawLine(pos.x+radius,pos.y+radius,pos.x-radius,pos.y+radius);
    p.drawLine(pos.x-radius,pos.y+radius,pos.x-radius,pos.y-radius);
    };
  for(const auto& [id,touch]:touches) {
    p.setPen(Pen(active,Painter::Alpha,2.f));
    p.setBrush(active);
    cross(touch.anchor,pad);
    p.drawLine(touch.anchor,touch.last);
    box(touch.last,pad);
    if(touch.role==Role::Move) {
      const int radius = movementRadius();
      p.setPen(Pen(gold,Painter::Alpha,2.f));
      p.setBrush(gold);
      box(touch.anchor,radius);
      box(touch.anchor,int(float(radius)*DirectionThreshold));
      label(touch.anchor.x-radius,touch.anchor.y+radius+pad,2*radius,
            "Inner box: direction threshold");
      }
    }
  }

void TouchInput::resizeEvent(Tempest::SizeEvent&) {
  // A moved layout must not apply the selection from a gesture in the old viewport.
  reset();
  }

void TouchInput::mouseDownEvent(Tempest::MouseEvent& e) {
  if(!touchEnabled) {
    // Consume gameplay touches instead of forwarding them as desktop mouse input.
    // Android's text editor receives its own input outside this widget.
    e.accept();
    return;
    }

  Touch touch;
  touch.anchor = e.pos();
  touch.last   = e.pos();
  touch.pressedAt = Application::tickCount();

  int button = -1;
  for(size_t i=0;i<std::size(Buttons);++i)
    if(buttonRect(i).contains(e.pos()))
      button = int(i);

  const bool onBlock=blockVisible() && blockRect().contains(e.pos());
  multiTap.down(e.mouseID,float(e.x),float(e.y),touch.pressedAt,
                gesturesEnabled && !uiActive && wheelPointer<0 && button<0 && !onBlock);
  if(tapCaptured || multiTap.ready()) {
    captureTap(e.mouseID,touch);
    return;
    }
  if(wheelPointer>=0 || gestureActive())
    return;
  if(button<0 && !onBlock && tryGesture(e.mouseID,touch))
    return;

  if(onBlock) {
    if(blockPointer>=0)
      return;
    blockPointer = e.mouseID;
    touch.role = Role::Button;
    touch.command = Command::Block;
    touch.actionSent = true;
    command(Command::Block,true);
    }
  else if(button>=0) {
    touch.role = Role::Button;
    touch.command = Buttons[button];
    touch.pendingAction = touch.command==Command::Accept && canLock && !uiActive;
    touch.pendingWheel = !uiActive && (touch.command==Command::Weapon || touch.command==Command::Back);
    if(!touch.pendingAction && !touch.pendingWheel) {
      touch.actionSent = true;
      command(touch.command,true);
      }
    }
  else if(e.x<w()/2) {
    if(movePointer>=0)
      return;
    touch.role = Role::Move;
    movePointer = e.mouseID;
    }
  else {
    if(lookPointer>=0)
      return;
    touch.role = Role::Look;
    lookPointer = e.mouseID;
    }
  touches[e.mouseID] = touch;
  if(debugOverlay || blockVisible())
    update();
  }

void TouchInput::mouseDragEvent(Tempest::MouseEvent& e) {
  multiTap.move(e.mouseID,float(e.x),float(e.y),tapSlop());
  auto it = touches.find(e.mouseID);
  if(it==touches.end())
    return;
  auto& touch = it->second;
  if(touch.role==Role::MultiTap) {
    touch.last=e.pos();
    update();
    return;
    }
  if(touch.role==Role::Gesture) {
    touch.last=e.pos();
    updateGesture();
    update();
    return;
    }
  if(wheelPointer>=0) {
    if(e.mouseID==wheelPointer) {
      moveWheel(touch,e.pos());
      }
    return;
    }
  if(touch.pendingAction) {
    const auto delta = e.pos()-touch.anchor;
    const float distance = std::hypot(float(delta.x),float(delta.y));
    const float threshold = float(std::max(32,std::min(w(),h())/18));
    if(Application::tickCount()-touch.pressedAt>=ActionHoldMs) {
      touch.pendingAction = false;
      touch.actionSent = true;
      command(Command::Accept,true);
      }
    else if(distance>=threshold) {
      touch.pendingAction = false;
      touch.command = Command::LockTarget;
      command(Command::LockTarget,true);
      }
    }
  if(touch.role==Role::Move)
    updateMovement(e.pos());
  else if(touch.role==Role::Look) {
    lookDelta += e.pos()-touch.last;
    }
  touch.last = e.pos();
  if(debugOverlay || blockVisible())
    update();
  }

void TouchInput::mouseUpEvent(Tempest::MouseEvent& e) {
  multiTap.move(e.mouseID,float(e.x),float(e.y),tapSlop());
  const int tap=multiTap.up(e.mouseID,Application::tickCount());
  if(tapCaptured) {
    touches.erase(e.mouseID);
    if(multiTap.empty()) {
      tapCaptured=false;
      // Decide only after every finger is lifted: four fingers must never save first.
      if(tap==3) command(Command::QuickSave,true);
      if(tap==4) command(Command::QuickLoad,true);
      }
    update();
    return;
    }
  // Recognize long holds even when Android batches the last move and release.
  tick();
  auto it = touches.find(e.mouseID);
  if(it==touches.end())
    return;
  if(it->second.role==Role::Gesture) {
    it->second.last=e.pos();
    updateGesture();
    releaseGesture();
    if(e.mouseID==gestureFirst) gestureFirst=-1;
    if(e.mouseID==gestureSecond) gestureSecond=-1;
    touches.erase(it);
    update();
    return;
    }
  if(e.mouseID==wheelPointer) {
    const auto action=it->second.command;
    moveWheel(it->second,e.pos());
    wheelPointer=-1;
    touches.erase(it);
    wheel(action,WheelPhase::Apply,e.pos());
    update();
    return;
    }
  if(it->second.role==Role::Move) {
    movePointer = -1;
    moveAxis = PointF();
    setDirection(Command::Up,false);
    setDirection(Command::Down,false);
    setDirection(Command::Left,false);
    setDirection(Command::Right,false);
    }
  else if(it->second.role==Role::Look) {
    lookPointer = -1;
    }
  else {
    if(it->second.command==Command::Block)
      blockPointer = -1;
    if(it->second.pendingWheel) {
      const auto action=it->second.command;
      touches.erase(it);
      command(action,true);
      command(action,false);
      update();
      return;
      }
    if(it->second.pendingAction)
      command(Command::TapAccept,true);
    else if(it->second.actionSent)
      command(it->second.command,false);
    }
  touches.erase(it);
  if(debugOverlay || blockVisible())
    update();
  }

void TouchInput::setTouchEnabled(bool enabled) {
  if(touchEnabled==enabled)
    return;
  touchEnabled = enabled;
  if(!enabled)
    reset();
  update();
  }

void TouchInput::setGesturesEnabled(bool enabled) {
  if(gesturesEnabled==enabled) return;
  gesturesEnabled=enabled;
  if(!enabled && (gestureActive() || tapCaptured)) reset();
  update();
  }

void TouchInput::setDebugOverlay(bool enabled) {
  if(debugOverlay==enabled)
    return;
  debugOverlay = enabled;
  update();
  }

void TouchInput::setAnalogMovement(bool enabled) {
  if(analogMovement==enabled)
    return;
  analogMovement = enabled;
  for(size_t i=0;i<4;++i)
    setDirection(Command(i),false);
  if(!enabled && movePointer>=0)
    updateMovement(touches.at(movePointer).last);
  }

void TouchInput::setDebugContext(bool classic, bool ui, bool lockAllowed, bool locked, bool blockAllowed) {
  if(classicCombat==classic && uiActive==ui && canLock==lockAllowed && targetLocked==locked && canBlock==blockAllowed)
    return;
  classicCombat = classic;
  uiActive = ui;
  canLock = lockAllowed;
  targetLocked = locked;
  canBlock = blockAllowed;
  if(uiActive && (gestureActive() || tapCaptured)) reset();
  if(uiActive || !canLock) {
    for(auto& [id,touch]:touches)
      touch.pendingAction = false;
    }
  if(uiActive)
    for(auto& [id,touch]:touches)
      touch.pendingWheel=false;
  if(!blockVisible()) {
    for(auto& [id,touch]:touches) {
      if(touch.command==Command::Block && touch.actionSent) {
        touch.actionSent = false;
        command(Command::Block,false);
        }
      }
    blockPointer = -1;
    }
  update();
  }

Rect TouchInput::buttonRect(size_t index) const {
  // Hit testing and the optional debug overlay use exactly the same bounds.
  const int left = w()*(index==4 ? ActionBoundaryPercent : LookBoundaryPercent)/100;
  const int top = h()*ButtonEdgesPercent[index]/100;
  const int bottom = h()*ButtonEdgesPercent[index+1]/100;
  return Rect(left,top,w()-left,bottom-top);
  }

bool TouchInput::blockVisible() const {
  return touchEnabled && !classicCombat && !uiActive && canBlock;
  }

Rect TouchInput::blockRect() const {
  const float scale = std::min(Gothic::interfaceScale(this),float(h())/480.f);
  const int size = std::max(56,int(72*scale));
  const int pad = std::max(8,int(12*scale));
  return Rect(buttonRect(4).x-pad-size,h()-pad-size,size,size);
  }

void TouchInput::drawBlock(Painter& p) const {
  const auto rect = blockRect();
  const bool pressed = blockPointer>=0;
  p.setBrush(pressed ? Color(0.45f,0.30f,0.08f,0.6f) : Color(0.025f,0.02f,0.015f,0.4f));
  p.drawRect(rect);
  const auto gold = pressed ? Color(1.f,0.85f,0.4f,1.f) : Color(0.843f,0.761f,0.631f,0.9f);
  p.setBrush(gold);
  p.setPen(Pen(gold,Painter::Alpha,2.f));
  p.drawLine(rect.x,rect.y,rect.x+rect.w,rect.y);
  p.drawLine(rect.x+rect.w,rect.y,rect.x+rect.w,rect.y+rect.h);
  p.drawLine(rect.x+rect.w,rect.y+rect.h,rect.x,rect.y+rect.h);
  p.drawLine(rect.x,rect.y+rect.h,rect.x,rect.y);

  // Original vector parry symbol; no game texture or external icon asset is bundled.
  const int icon = (rect.w*3)/4;
  const int x = rect.x+(rect.w-icon)/2;
  const int y = rect.y+rect.h/16;
  auto stroke = [&](int x0,int y0,int x1,int y1) {
    p.drawLine(x+x0*icon/32,y+y0*icon/32,x+x1*icon/32,y+y1*icon/32);
    };
  auto swords = [&]() {
    stroke(7,25,25,7);
    stroke(25,7,23,13);
    stroke(25,7,19,9);
    stroke(8,18,14,24);
    stroke(25,25,7,7);
    stroke(7,7,9,13);
    stroke(7,7,13,9);
    stroke(18,24,24,18);
    };
  const auto outline = Color(0.06f,0.045f,0.025f,1.f);
  p.setBrush(outline);
  p.setPen(Pen(outline,Painter::Alpha,float(icon)/12.f));
  swords();
  p.setBrush(gold);
  p.setPen(Pen(gold,Painter::Alpha,std::max(2.f,float(icon)/28.f)));
  swords();
  const auto& font = Resources::font(float(rect.w)/90.f);
  font.drawText(p,rect.x,rect.y+rect.h-font.pixelSize()-rect.h/16,rect.w,font.pixelSize()*2,"BLOCK",AlignHCenter);
  }

void TouchInput::tick() {
  const auto now = Application::tickCount();
  if(wheelPointer>=0) {
    auto& touch=touches.at(wheelPointer);
    moveWheel(touch,touch.last);
    return;
    }
  for(auto& [id,touch]:touches) {
    if(touch.pendingWheel && now-touch.pressedAt>=WheelHoldMs) {
      startWheel(id);
      return;
      }
    if(touch.pendingAction && now-touch.pressedAt>=ActionHoldMs) {
      touch.pendingAction = false;
      touch.actionSent = true;
      command(Command::Accept,true);
      }
    }
  }

void TouchInput::startWheel(int pointer) {
  auto touch=touches.at(pointer);
  // Release other held controls before the wheel becomes modal.
  reset();
  touch.pendingWheel=false;
  touch.pendingAction=false;
  touch.actionSent=false;
  touches[pointer]=touch;
  if(wheel(touch.command,WheelPhase::Begin,touch.anchor))
    wheelPointer=pointer;
  update();
  }

void TouchInput::moveWheel(Touch& touch, Point pos) {
  touch.last=pos;
  const auto delta=pos-touch.anchor;
  touch.wheelMoved |= std::hypot(float(delta.x),float(delta.y))>=12.f;
  // Ignore initial finger jitter, but always use the final release position after dragging.
  if(touch.wheelMoved)
    wheel(touch.command,WheelPhase::Move,pos);
  }

void TouchInput::cancelWheel() {
  if(wheelPointer>=0)
    reset();
  }

PointF TouchInput::movementAxis() const {
  return moveAxis;
  }

Point TouchInput::takeLookDelta() {
  auto ret = lookDelta;
  lookDelta = Point();
  return ret;
  }

bool TouchInput::isLooking() const {
  return lookPointer>=0;
  }

void TouchInput::updateMovement(const Point& pos) {
  auto it = touches.find(movePointer);
  if(it==touches.end())
    return;
  const auto  delta  = pos-it->second.anchor;
  const float radius = float(movementRadius());
  moveAxis.x = std::clamp(float(delta.x)/radius,-1.f,1.f);
  moveAxis.y = std::clamp(float(delta.y)/radius,-1.f,1.f);

  if(analogMovement)
    return;
  setDirection(Command::Up,   moveAxis.y < -DirectionThreshold);
  setDirection(Command::Down, moveAxis.y >  DirectionThreshold);
  setDirection(Command::Left, moveAxis.x < -DirectionThreshold);
  setDirection(Command::Right,moveAxis.x >  DirectionThreshold);
  }

int TouchInput::movementRadius() const {
  return std::max(80,std::min(w(),h())/6);
  }

void TouchInput::setDirection(Command value, bool pressed) {
  const auto id = size_t(value);
  if(id>=4 || directions[id]==pressed)
    return;
  directions[id] = pressed;
  command(value,pressed);
  }

void TouchInput::reset() {
  multiTap.reset();
  tapCaptured=false;
  releaseGesture();
  gestureFirst=-1;
  gestureSecond=-1;
  gestureFired=false;
  if(wheelPointer>=0) {
    const auto action=touches.at(wheelPointer).command;
    wheelPointer=-1;
    wheel(action,WheelPhase::Cancel,Point());
    }
  for(auto& touch:touches)
    if(touch.second.role==Role::Button && touch.second.actionSent)
      command(touch.second.command,false);
  for(size_t i=0;i<4;++i)
    setDirection(Command(i),false);
  touches.clear();
  moveAxis = PointF();
  lookDelta = Point();
  movePointer = -1;
  lookPointer = -1;
  blockPointer = -1;
  analogMovement = false;
  }

bool TouchInput::tryGesture(int pointer, const Touch& second) {
  if(!gesturesEnabled || uiActive || touches.size()!=1) return false;
  auto& [firstId,first]=*touches.begin();
  if(first.role!=Role::Move && first.role!=Role::Look) return false;
  if((first.anchor.x<w()/2)!=(second.anchor.x<w()/2)) return false;
  const auto travel=first.last-first.anchor;
  const float slop=float(std::max(12,std::min(w(),h())/90));
  if(!TwoFingerSwipe::canPair(second.pressedAt-first.pressedAt,std::hypot(float(travel.x),float(travel.y)),slop))
    return false;
  gestureFirst=firstId;
  gestureSecond=pointer;
  gestureStarted=second.pressedAt;
  gestureFired=false;
  first.role=Role::Gesture;
  auto touch=second;
  touch.role=Role::Gesture;
  touches[pointer]=touch;
  movePointer=-1;
  lookPointer=-1;
  moveAxis=PointF();
  lookDelta=Point();
  for(size_t i=0;i<4;++i) setDirection(Command(i),false);
  update();
  return true;
  }

void TouchInput::updateGesture() {
  if(gestureFired || gestureFirst<0 || gestureSecond<0) return;
  const auto& first=touches.at(gestureFirst);
  const auto& second=touches.at(gestureSecond);
  const auto a=first.last-first.anchor, b=second.last-second.anchor;
  const float threshold=float(std::max(48,std::min(w(),h())/18));
  const int direction=TwoFingerSwipe::direction(float(a.x),float(a.y),float(b.x),float(b.y),threshold,
                                               Application::tickCount()-gestureStarted);
  if(direction==0) return;
  gestureFired=true;
  if(first.anchor.x<w()/2) {
    if(direction<0) command(Command::FirstPerson,true);
    else {
      gestureLookBehind=true;
      command(Command::LookBehind,true);
      }
    }
  else command(direction<0 ? Command::SneakOff : Command::SneakOn,true);
  }

void TouchInput::releaseGesture() {
  if(gestureLookBehind) {
    gestureLookBehind=false;
    command(Command::LookBehind,false);
    }
  }

float TouchInput::tapSlop() const {
  return float(std::max(16,std::min(w(),h())/60));
  }

void TouchInput::captureTap(int pointer, const Touch& touch) {
  releaseGesture();
  gestureFirst=-1;
  gestureSecond=-1;
  gestureFired=false;
  tapCaptured=true;
  touches[pointer]=touch;
  for(auto& [id,held]:touches) held.role=Role::MultiTap;
  movePointer=-1;
  lookPointer=-1;
  moveAxis=PointF();
  lookDelta=Point();
  for(size_t i=0;i<4;++i) setDirection(Command(i),false);
  update();
  }
