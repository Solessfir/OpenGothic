#include "touchinput.h"

#include <Tempest/SystemApi>

#include <algorithm>
#include <cmath>

using namespace Tempest;

TouchInput::TouchInput(CommandHandler command)
  :command(std::move(command)) {
  }

void TouchInput::paintEvent(Tempest::PaintEvent& e) {
  (void)e;
  }

void TouchInput::mouseDownEvent(Tempest::MouseEvent& e) {
  if(!touchEnabled || SystemApi::gamepadState().connected) {
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
  else if(e.x<(w()*84)/100 && lookPointer<0) {
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
    touch.last = e.pos();
    }
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
  }

void TouchInput::setTouchEnabled(bool enabled) {
  if(touchEnabled==enabled)
    return;
  touchEnabled = enabled;
  if(!enabled)
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
  const float radius = float(std::max(80,std::min(w(),h())/6));
  moveAxis.x = std::clamp(float(delta.x)/radius,-1.f,1.f);
  moveAxis.y = std::clamp(float(delta.y)/radius,-1.f,1.f);

  constexpr float threshold = 0.35f;
  setDirection(Command::Up,   moveAxis.y < -threshold);
  setDirection(Command::Down, moveAxis.y >  threshold);
  setDirection(Command::Left, moveAxis.x < -threshold);
  setDirection(Command::Right,moveAxis.x >  threshold);
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
