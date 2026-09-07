#include "touchinput.h"

#include <Tempest/Painter>
#include <Tempest/Application>

#include "gothic.h"
#include "resources.h"
#include "utils/gthfont.h"

#include <algorithm>
#include <cmath>

using namespace Tempest;

TouchInput::TouchInput(CommandHandler command)
  :command(std::move(command)) {
  }

void TouchInput::paintEvent(Tempest::PaintEvent& e) {
  if(!debugOverlay && !blockVisible())
    return;

  Painter p(e);
  if(blockVisible())
    drawBlock(p);
  if(!debugOverlay)
    return;
  const auto gold = Color(0.843f,0.761f,0.631f,0.75f);
  const auto active = Color(1.f,0.85f,0.4f,0.95f);
  const float scale = std::min(Gothic::interfaceScale(this),float(h())/480.f);
  const auto& font = Resources::font(scale);
  const int pad = std::max(8,int(12*scale));
  const int line = font.pixelSize();
  const int moveEnd = w()/2;
  const int lookEnd = (w()*LookBoundaryPercent)/100;
  p.setPen(Pen(gold,Painter::Alpha,2.f));
  p.setBrush(gold);
  p.drawLine(moveEnd,0,moveEnd,h());
  p.drawLine(lookEnd,0,lookEnd,h());

  auto label = [&](int x,int y,int width,std::string_view text) {
    font.drawText(p,x,y,width,3*line,text,AlignHCenter);
    };
  label(pad,2*line,moveEnd-2*pad,uiActive ? "MENU DIRECTIONS" : "MOVE / TURN");
  label(moveEnd+pad,2*line,lookEnd-moveEnd-2*pad,"CAMERA DRAG");
  label(pad,4*line,lookEnd-2*pad,!touchEnabled ? "Gamepad active: virtual touch zones inactive" :
        (uiActive ? "Touch debug: menu" : (classicCombat ? "Touch debug: Gothic 1 controls" : "Touch debug: Gothic 2 controls")));
  if(!touchEnabled)
    label(pad,6*line,lookEnd-2*pad,"Touch still reaches normal mouse / UI input");
  else if(!uiActive && classicCombat) {
    label(pad,6*line,lookEnd-2*pad,"Hold ACTION, then move stick: up = attack, down = block");
    label(pad,7*line,lookEnd-2*pad,"Left / right = side attacks with melee weapon drawn");
    }

  const char* names[] = {"BACK / MENU","INVENTORY","JUMP","DRAW / SHEATHE",
                        uiActive ? "ACCEPT" : (classicCombat ? "HOLD ACTION" : "USE / ATTACK")};
  for(int i=0;i<5;++i) {
    const int top = (h()*i)/5;
    const int bottom = (h()*(i+1))/5;
    bool pressed = false;
    for(const auto& [id,touch]:touches)
      pressed |= touch.role==Role::Button && touch.command==Buttons[i];
    p.setBrush(pressed ? Color(0.8f,0.6f,0.2f,0.32f) : Color(0.04f,0.03f,0.02f,0.18f));
    p.drawRect(lookEnd,top,w()-lookEnd,bottom-top);
    p.setBrush(gold);
    p.drawLine(lookEnd,top,w(),top);
    label(lookEnd+pad,top+(bottom-top)/2-line,w()-lookEnd-2*pad,names[i]);
    if(i==4 && canLock && touchEnabled && !uiActive)
      label(lookEnd+pad,top+(bottom-top)/2+line,w()-lookEnd-2*pad,targetLocked ? "DRAG: UNLOCK" : "DRAG: LOCK");
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

void TouchInput::mouseDownEvent(Tempest::MouseEvent& e) {
  if(!touchEnabled) {
    e.ignore();
    return;
    }

  Touch touch;
  touch.anchor = e.pos();
  touch.last   = e.pos();
  touch.pressedAt = Application::tickCount();

  if(blockVisible() && blockRect().contains(e.pos())) {
    if(blockPointer>=0)
      return;
    blockPointer = e.mouseID;
    touch.role = Role::Button;
    touch.command = Command::Block;
    touch.actionSent = true;
    command(Command::Block,true);
    }
  else if(e.x<w()/2 && movePointer<0) {
    touch.role  = Role::Move;
    movePointer = e.mouseID;
    }
  else if(e.x<(w()*LookBoundaryPercent)/100 && lookPointer<0) {
    touch.role  = Role::Look;
    lookPointer = e.mouseID;
    }
  else {
    touch.role = Role::Button;
    touch.command = Buttons[std::clamp((e.y*5)/std::max(h(),1),0,4)];
    touch.pendingAction = touch.command==Command::Accept && canLock && !uiActive;
    if(!touch.pendingAction) {
      touch.actionSent = true;
      command(touch.command,true);
      }
    }
  touches[e.mouseID] = touch;
  if(debugOverlay || blockVisible())
    update();
  }

void TouchInput::mouseDragEvent(Tempest::MouseEvent& e) {
  auto it = touches.find(e.mouseID);
  if(it==touches.end())
    return;
  auto& touch = it->second;
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
  auto it = touches.find(e.mouseID);
  if(it==touches.end())
    return;
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

void TouchInput::setDebugOverlay(bool enabled) {
  if(debugOverlay==enabled)
    return;
  debugOverlay = enabled;
  update();
  }

void TouchInput::setDebugContext(bool classic, bool ui, bool lockAllowed, bool locked, bool blockAllowed) {
  if(classicCombat==classic && uiActive==ui && canLock==lockAllowed && targetLocked==locked && canBlock==blockAllowed)
    return;
  classicCombat = classic;
  uiActive = ui;
  canLock = lockAllowed;
  targetLocked = locked;
  canBlock = blockAllowed;
  if(uiActive || !canLock) {
    for(auto& [id,touch]:touches)
      touch.pendingAction = false;
    }
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

bool TouchInput::blockVisible() const {
  return touchEnabled && !classicCombat && !uiActive && canBlock;
  }

Rect TouchInput::blockRect() const {
  const float scale = std::min(Gothic::interfaceScale(this),float(h())/480.f);
  const int size = std::max(56,int(72*scale));
  const int pad = std::max(8,int(12*scale));
  return Rect((w()*LookBoundaryPercent)/100-pad-size,h()-pad-size,size,size);
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
  for(auto& [id,touch]:touches) {
    if(touch.pendingAction && now-touch.pressedAt>=ActionHoldMs) {
      touch.pendingAction = false;
      touch.actionSent = true;
      command(Command::Accept,true);
      }
    }
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
  }
