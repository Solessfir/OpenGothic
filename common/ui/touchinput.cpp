#include "touchinput.h"

#include <Tempest/Painter>

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
  if(!debugOverlay)
    return;

  Painter p(e);
  const auto gold = Color(0.843f,0.761f,0.631f,0.75f);
  const auto active = Color(1.f,0.85f,0.4f,0.95f);
  const float scale = std::min(Gothic::interfaceScale(this),float(h())/480.f);
  const auto& font = Resources::font(scale);
  const int pad = std::max(8,int(12*scale));
  const int line = font.pixelSize();
  const int moveEnd = w()/2;
  const int lookEnd = (w()*LookBoundaryPercent)/100;
  p.setPen(Pen(gold,Painter::Alpha,2.f));
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

  constexpr Command buttons[] = {Command::Back,Command::Inventory,Command::Weapon,Command::Jump,Command::Accept};
  const char* names[] = {"BACK / MENU","INVENTORY","DRAW / SHEATHE","JUMP",
                        uiActive ? "ACCEPT" : (classicCombat ? "HOLD ACTION" : "USE / ATTACK")};
  for(int i=0;i<5;++i) {
    const int top = (h()*i)/5;
    const int bottom = (h()*(i+1))/5;
    bool pressed = false;
    for(const auto& [id,touch]:touches)
      pressed |= touch.role==Role::Button && touch.command==buttons[i];
    p.setBrush(pressed ? Color(0.8f,0.6f,0.2f,0.32f) : Color(0.04f,0.03f,0.02f,0.18f));
    p.drawRect(lookEnd,top,w()-lookEnd,bottom-top);
    p.drawLine(lookEnd,top,w(),top);
    label(lookEnd+pad,top+(bottom-top)/2-line,w()-lookEnd-2*pad,names[i]);
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
    cross(touch.anchor,pad);
    p.drawLine(touch.anchor,touch.last);
    box(touch.last,pad);
    if(touch.role==Role::Move) {
      const int radius = movementRadius();
      p.setPen(Pen(gold,Painter::Alpha,2.f));
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

  if(e.x<w()/2 && movePointer<0) {
    touch.role  = Role::Move;
    movePointer = e.mouseID;
    }
  else if(e.x<(w()*LookBoundaryPercent)/100 && lookPointer<0) {
    touch.role  = Role::Look;
    lookPointer = e.mouseID;
    }
  else {
    touch.role = Role::Button;
    if(e.y<(h()*20)/100)
      touch.command = Command::Back;
    else if(e.y<(h()*40)/100)
      touch.command = Command::Inventory;
    else if(e.y<(h()*60)/100)
      touch.command = Command::Weapon;
    else if(e.y<(h()*80)/100)
      touch.command = Command::Jump;
    else
      touch.command = Command::Accept;
    command(touch.command,true);
    }
  touches[e.mouseID] = touch;
  if(debugOverlay)
    update();
  }

void TouchInput::mouseDragEvent(Tempest::MouseEvent& e) {
  auto it = touches.find(e.mouseID);
  if(it==touches.end())
    return;
  auto& touch = it->second;
  if(touch.role==Role::Move)
    updateMovement(e.pos());
  else if(touch.role==Role::Look) {
    lookDelta += e.pos()-touch.last;
    }
  touch.last = e.pos();
  if(debugOverlay)
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
    command(it->second.command,false);
    }
  touches.erase(it);
  if(debugOverlay)
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

void TouchInput::setDebugContext(bool classic, bool ui) {
  if(classicCombat==classic && uiActive==ui)
    return;
  classicCombat = classic;
  uiActive = ui;
  if(debugOverlay)
    update();
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
    if(touch.second.role==Role::Button)
      command(touch.second.command,false);
  for(size_t i=0;i<4;++i)
    setDirection(Command(i),false);
  touches.clear();
  moveAxis = PointF();
  lookDelta = Point();
  movePointer = -1;
  lookPointer = -1;
  }
