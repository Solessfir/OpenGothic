#include "touchinput.h"

#include <Tempest/Painter>
#include <Tempest/Application>

#include "gothic.h"
#include "resources.h"
#include "utils/gthfont.h"
#include "utils/twofingerswipe.h"
#include "utils/touchmovement.h"

#include "touchasset.h"

#include <algorithm>
#include <cmath>

using namespace Tempest;

TouchInput::TouchInput(CommandHandler command, WheelHandler wheel, AdjustmentHandler adjust)
  :command(std::move(command)),wheel(std::move(wheel)),adjustment(std::move(adjust)) {
  }

void TouchInput::setMenuAdjustment(bool enabled) {
  if(menuAdjustment==enabled) return;
  menuAdjustment=enabled;
  if(!enabled && adjustmentPointer>=0) {
    touches.erase(adjustmentPointer);
    adjustmentPointer=-1;
    adjustmentDrag.reset();
    }
  update();
  }

void TouchInput::setLockPicking(bool enabled) {
  if(lockPicking==enabled) return;
  lockPicking=enabled;
  update();
  }

void TouchInput::setDebugLeftInset(int inset) {
  if(debugLeftInset==inset)
    return;
  debugLeftInset = inset;
  update();
  }

void TouchInput::setDebugSafeArea(Rect area) {
  if(debugSafeArea==area) return;
  debugSafeArea=area;
  update();
  }

void TouchInput::paintEvent(Tempest::PaintEvent& e) {
  Painter p(e);
  if(buttonOpacity>0.f && touchEnabled && !buttonsHidden)
    drawControls(p);
  if(debugOverlay)
    drawDebug(p);
  }

void TouchInput::drawDebug(Painter& p) {
  const auto gold = Color(0.843f,0.761f,0.631f,0.28f);
  const auto active = Color(1.f,0.85f,0.4f,0.95f);
  const float scale = 1.4f*std::min(Gothic::interfaceScale(this),float(h())/720.f);
  const auto& font = Resources::font(scale);
  const auto& detail = Resources::font(scale*0.92f);
  const int pad = std::max(8,int(12*scale));
  const int line = font.pixelSize();
  const int moveEnd = w()/2;
  const int lookEnd = (w()*3)/4;
  p.setPen(Pen(gold,Painter::Alpha,std::max(1.f,scale)));
  p.setBrush(gold);
  p.drawLine(moveEnd,0,moveEnd,h());

  const auto safe=debugSafeArea.isEmpty() ? Rect(0,0,w(),h()) : debugSafeArea;
  const int leftX=safe.x+pad;
  const int leftWidth=moveEnd-pad-leftX, rightX=moveEnd+pad, rightWidth=lookEnd-moveEnd-2*pad;
  const int baseline=safe.y+pad+line;
  font.drawTextShadow(p,leftX+debugLeftInset,baseline,std::max(1,leftWidth-debugLeftInset),line,uiActive ? "NAVIGATE" : "MOVE / TURN");
  font.drawTextShadow(p,rightX,baseline,rightWidth,line,uiActive ? (menuAdjustment ? "ADJUST VALUE" : "NAVIGATE") : "LOOK");
  detail.drawTextShadow(p,leftX,baseline+2*line,leftWidth,line,
                        uiActive ? "Touch controls / Menus" : (classicCombat ? "Touch controls / Classic combat" : "Touch controls / Modern combat"));
  if(!uiActive)
    detail.drawTextShadow(p,rightX,baseline+2*line,rightWidth,line,"Drag to move the camera");
  else
    detail.drawTextShadow(p,rightX,baseline+2*line,rightWidth,2*line,
                          lockPicking ? "Left / right: turn pick" : (menuAdjustment ? "Drag left / right" : "Right: accept / Left: back"));
  int legendY=baseline+5*line;
  auto legend=[&](std::string_view text,bool heading=false) {
    const auto& f=heading ? font : detail;
    f.drawTextShadow(p,leftX,legendY,leftWidth,2*line,text);
    legendY+=f.textSize(leftWidth,text).h+std::max(4,int(6.f*scale));
    };
  if(!touchEnabled)
    legend("Gamepad active / Keyboard available");
  else if(lockPicking) {
    legend("LOCKPICKING",true);
    legend("Left / right: turn pick");
    legend("Center between turns");
    legend("Down / Back: stop picking");
    }
  else if(uiActive) {
    legend("NAVIGATION",true);
    legend("Up / down: select");
    legend("Left: back / Right: accept");
    if(saveDeleteEnabled) legend("3-finger tap: delete save");
    }
  else if(gesturesEnabled) {
    legend("GESTURES",true);
    legend("Left side: 2 fingers down / up - sneak / stand");
    legend("Right side: 2 fingers up - first person");
    legend("Right side: 2 fingers down + hold - look behind");
    legend("Anywhere: 2 fingers left / right - health / mana");
    legend("Anywhere: 3-finger tap - save / 4-finger tap - load");
    if(classicCombat) {
      legendY+=line;
      legend("CLASSIC COMBAT",true);
      legend("Hold Action + movement up / sides: attack");
      legend("Hold Action + pull straight down: block");
      }
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
      if(classicCombat && canBlock && !uiActive) {
        const int top=int(float(radius)*0.65f);
        const int topWidth=int(float(top)*0.40f), bottomWidth=int(float(radius)*0.40f);
        p.drawLine(touch.anchor.x-topWidth,touch.anchor.y+top,touch.anchor.x+topWidth,touch.anchor.y+top);
        p.drawLine(touch.anchor.x-topWidth,touch.anchor.y+top,touch.anchor.x-bottomWidth,touch.anchor.y+radius);
        p.drawLine(touch.anchor.x+topWidth,touch.anchor.y+top,touch.anchor.x+bottomWidth,touch.anchor.y+radius);
        p.drawLine(touch.anchor.x-bottomWidth,touch.anchor.y+radius,touch.anchor.x+bottomWidth,touch.anchor.y+radius);
        }
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

  if(onTouchStarted) onTouchStarted();

  Touch touch;
  touch.anchor = e.pos();
  touch.last   = e.pos();
  touch.pressedAt = Application::tickCount();

  int button = buttonAt(e.pos());
  // Jump does nothing in the water, so a touch there turns the camera instead.
  if(swimming && !uiActive && button>=0 && Buttons[button]==Command::Jump)
    button = -1;

  const bool onBlock=button>=0 && Buttons[button]==Command::Block;
  multiTap.down(e.mouseID,float(e.x),float(e.y),touch.pressedAt,
                (saveDeleteEnabled || (gesturesEnabled && !uiActive)) && wheelPointer<0);
  if(tapCaptured || multiTap.ready()) {
    captureTap(e.mouseID,touch);
    return;
    }
  if(wheelPointer>=0 || gestureActive())
    return;
  if(tryGesture(e.mouseID,touch))
    return;

  if(onBlock) {
    if(blockPointer>=0)
      return;
    blockPointer = e.mouseID;
    touch.role = Role::Button;
    touch.command = Command::Block;
    touch.pendingButton = multiTap.joining(touch.pressedAt);
    if(!touch.pendingButton) {
      touch.actionSent = true;
      command(Command::Block,true);
      }
    }
  else if(button>=0) {
    touch.role = Role::Button;
    touch.command = Buttons[button];
    touch.pendingAction = touch.command==Command::Accept && canLock && !uiActive;
    touch.pendingWheel = !uiActive && (touch.command==Command::Weapon || touch.command==Command::Back ||
                                      touch.command==Command::Inventory);
    if(!touch.pendingAction && !touch.pendingWheel) {
      touch.pendingButton = multiTap.joining(touch.pressedAt);
      if(!touch.pendingButton) {
        touch.actionSent = true;
        command(touch.command,true);
        }
      }
    }
  else if(uiActive && onItemTouch && onItemTouch(ItemPhase::Begin,e.pos())) {
    touch.role = Role::Item;
    }
  else if(uiActive && menuAdjustment && e.x>=w()/2) {
    if(adjustmentPointer>=0) return;
    touch.role=Role::Adjust;
    adjustmentPointer=e.mouseID;
    adjustmentDrag.reset();
    }
  else if(e.x<w()/2 || uiActive) {
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
  update();
  }

void TouchInput::mouseDragEvent(Tempest::MouseEvent& e) {
  multiTap.move(e.mouseID,float(e.x),float(e.y),tapSlop());
  auto it = touches.find(e.mouseID);
  if(it==touches.end())
    return;
  auto& touch = it->second;
  const auto travel = e.pos()-touch.anchor;
  touch.moved |= std::hypot(float(travel.x),float(travel.y))>=tapSlop();
  if(touch.role==Role::MultiTap) {
    touch.last=e.pos();
    update();
    return;
    }
  if(touch.role==Role::Item) {
    touch.last=e.pos();
    if(onItemTouch)
      onItemTouch(ItemPhase::Move,e.pos());
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
    if(Application::tickCount()-touch.pressedAt>=ActionHoldMs && !multiTap.joining(Application::tickCount())) {
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
  else if(touch.role==Role::Adjust) {
    adjustValue(touch,e.pos());
    return;
    }
  else if(touch.role==Role::Look) {
    lookDelta += e.pos()-touch.last;
    }
  touch.last = e.pos();
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
      switch(MultiFingerTap::action(tap,saveDeleteEnabled)) {
        case MultiFingerTap::Action::QuickSave:  command(Command::QuickSave,true); break;
        case MultiFingerTap::Action::QuickLoad:  command(Command::QuickLoad,true); break;
        case MultiFingerTap::Action::DeleteSave: command(Command::DeleteSave,true); break;
        case MultiFingerTap::Action::None: break;
        }
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
    touches.erase(e.mouseID);
    update();
    return;
    }
  if(e.mouseID==wheelPointer) {
    const auto action=it->second.command;
    moveWheel(it->second,e.pos());
    wheelPointer=-1;
    touches.erase(e.mouseID);
    wheel(action,WheelPhase::Apply,e.pos());
    update();
    return;
    }
  // Callbacks below may open or close a menu, which resets every touch: take this one out of the map first.
  auto touch = it->second;
  touches.erase(it);
  if(touch.role==Role::Item) {
    if(onItemTouch)
      onItemTouch(touch.moved || touch.held ? ItemPhase::Release : ItemPhase::Tap,e.pos());
    update();
    return;
    }

  const auto travel = e.pos()-touch.anchor;
  // Swipes keep navigating; a finger lifted in place points at the menu item under it.
  const bool menuTap = uiActive && !touch.moved && std::hypot(float(travel.x),float(travel.y))<tapSlop() &&
                       (touch.role==Role::Adjust || touch.role==Role::Move);
  if(touch.role==Role::Adjust) {
    adjustValue(touch,e.pos());
    adjustmentPointer=-1;
    adjustmentDrag.reset();
    if(menuTap && onTap) onTap(e.pos());
    update();
    return;
    }
  if(touch.role==Role::Move) {
    movePointer = -1;
    moveAxis = PointF();
    setDirection(Command::Up,false);
    setDirection(Command::Down,false);
    setDirection(Command::Left,false);
    setDirection(Command::Right,false);
    if(menuTap && onTap) onTap(e.pos());
    }
  else if(touch.role==Role::Look) {
    lookPointer = -1;
    }
  else {
    if(touch.command==Command::Block)
      blockPointer = -1;
    if(touch.pendingWheel || touch.pendingButton) {
      command(touch.command,true);
      command(touch.command,false);
      update();
      return;
      }
    if(touch.pendingAction)
      command(Command::TapAccept,true);
    else if(touch.actionSent)
      command(touch.command,false);
    }
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

void TouchInput::setSaveDeleteEnabled(bool enabled) {
  if(saveDeleteEnabled==enabled) return;
  // Never reinterpret a tap that started in another menu or in gameplay.
  reset();
  saveDeleteEnabled=enabled;
  update();
  }

void TouchInput::setButtonStyle(bool labels, float opacity) {
  if(buttonLabels==labels && buttonOpacity==opacity) return;
  buttonLabels=labels;
  buttonOpacity=opacity;
  update();
  }

void TouchInput::setMenuOpen(bool open) {
  if(menuOpen==open) return;
  menuOpen=open;
  update();
  }

void TouchInput::setButtonsHidden(bool hidden) {
  if(buttonsHidden==hidden) return;
  buttonsHidden=hidden;
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

void TouchInput::setClassicAction(bool held) {
  if(classicAction==held) return;
  classicAction=held;
  if(movePointer>=0) updateMovement(touches.at(movePointer).last);
  }

void TouchInput::setDebugContext(bool classic, bool ui, bool lockAllowed, bool locked, bool blockAllowed) {
  if(classicCombat==classic && uiActive==ui && canLock==lockAllowed && targetLocked==locked && canBlock==blockAllowed)
    return;
  classicCombat = classic;
  if(uiActive!=ui)
    for(auto& [id,touch]:touches) touch.pendingButton=false;
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

float TouchInput::layoutUnit() const {
  // Fit a 1280x720 reference screen, so nearly square screens like unfolded foldables keep the buttons at the edge
  return std::min(float(h())/720.f,float(w())/1280.f);
  }

TouchInput::ButtonShape TouchInput::buttonShape(size_t index) const {
  // Laid out on a 1280x720 reference screen: Attack in the bottom-right corner, three actions on an arc around it
  // within the thumb's reach, and the two buttons with hold wheels in the top-right corner.
  const auto  m      = margins();
  const float right  = float(w()-m.right);
  const float top    = float(m.top);
  const float bottom = float(h()-m.bottom);
  const float u      = layoutUnit();
  const PointF attack(right-100.f*u,bottom-114.f*u);
  auto arc = [&](float degrees) {
    const float a = degrees*3.14159265f/180.f;
    return PointF(attack.x+255.f*u*std::cos(a),attack.y-255.f*u*std::sin(a));
    };
  switch(Buttons[index]) {
    case Command::Back:      return {PointF(right-52.f*u,top+56.f*u),28.f*u};
    case Command::Inventory: return {PointF(right-52.f*u,top+156.f*u),28.f*u};
    case Command::Block:     return {arc(180.f),42.f*u};
    case Command::Jump:      return {arc(140.f),42.f*u};
    case Command::Weapon:    return {arc(100.f),42.f*u};
    default:                 return {attack,78.f*u};
    }
  }

bool TouchInput::buttonShown(size_t index) const {
  const auto c = Buttons[index];
  // The inventory needs no buttons: cells are tapped directly and a tap on free space closes it
  if(uiActive)
    return !inventoryNavigation && (c==Command::Back || c==Command::Accept);
  return c!=Command::Block || blockVisible();
  }

int TouchInput::buttonAt(Point pos) const {
  // A touch a little outside a button still counts, and the gap between two buttons goes to the nearer one.
  int   best = -1;
  float bestDistance = 1.35f;
  for(size_t i=0;i<std::size(Buttons);++i) {
    if(!buttonShown(i))
      continue;
    const auto  b = buttonShape(i);
    const float d = std::hypot(float(pos.x)-b.center.x,float(pos.y)-b.center.y)/b.radius;
    if(d<bestDistance) {
      bestDistance = d;
      best = int(i);
      }
    }
  return best;
  }

bool TouchInput::buttonPressed(Command c) const {
  for(const auto& [id,touch]:touches)
    if(touch.role==Role::Button && touch.command==c)
      return true;
  return false;
  }

bool TouchInput::blockVisible() const {
  return touchEnabled && !classicCombat && !uiActive && canBlock;
  }

void TouchInput::drawControls(Painter& p) {
  enum Image : uint8_t { Attack, Use, Block, Jump, Draw, Sheathe, Menu, Inventory, Pressed };
  if(textures.empty()) {
    for(auto name:{"touch_attack.png","touch_use.png","touch_block.png","touch_jump.png","touch_draw.png","touch_sheathe.png",
                   "touch_menu.png","touch_inventory.png","touch_pressed.png"})
      textures.push_back(Resources::loadTexturePm(TouchAsset::pixmap(name)));
    }
  // Nothing over game menus, videos and loading screens; dialogue and documents keep Back and Accept
  if(menuOpen)
    return;

  const float u     = layoutUnit();
  const float scale = std::min(Gothic::interfaceScale(this),u);
  auto        font  = Resources::font(scale);
  auto        small = Resources::font(scale*0.8f);
  const bool  armed = canLock;
  font.fade(buttonOpacity);
  small.fade(buttonOpacity);

  // The images leave room around the button: a circle fills 97% of its image, a rounded square 220 of 256 px.
  auto image = [&](Image i,PointF center,float size,float alpha=1.f) {
    const auto& t = textures[i];
    const int   s = int(size);
    p.setBrush(Brush(t,Color(1.f,1.f,1.f,alpha*buttonOpacity),PaintDevice::Alpha,ClampMode::ClampToEdge));
    p.drawRect(int(center.x-size*0.5f),int(center.y-size*0.5f),s,s,0,0,t.w(),t.h());
    };
  auto below = [&](PointF center,float radius,std::string_view text,std::string_view hint) {
    if(!buttonLabels)
      return;
    const int width = int(radius*4.f);
    const int y     = int(center.y+radius)+font.pixelSize()+int(2.f*u);
    font.drawTextShadow(p,int(center.x)-width/2,y,width,font.pixelSize(),text,AlignHCenter);
    if(!hint.empty())
      small.drawTextShadow(p,int(center.x)-width/2,y+small.pixelSize(),width,small.pixelSize(),hint,AlignHCenter);
    };
  auto left = [&](PointF center,float radius,std::string_view text,std::string_view hint) {
    if(!buttonLabels)
      return;
    const int width = int(240.f*u);
    const int x     = int(center.x-radius*1.4f)-width;
    const int y     = int(center.y)-(hint.empty() ? -font.pixelSize()/2 : 0);
    font.drawTextShadow(p,x,y,width,font.pixelSize(),text,AlignRight);
    if(!hint.empty())
      small.drawTextShadow(p,x,y+small.pixelSize(),width,small.pixelSize(),hint,AlignRight);
    };

  for(size_t i=0;i<std::size(Buttons);++i) {
    if(!buttonShown(i))
      continue;
    const auto c = Buttons[i];
    if(uiActive && c!=Command::Back && c!=Command::Accept)
      continue;
    const auto b = buttonShape(i);
    const float d = 2.f*b.radius;
    switch(c) {
      case Command::Back:
        image(Menu,b.center,d*256.f/220.f);
        left(b.center,b.radius,uiActive ? "Back" : "Menu",uiActive ? "" : "Hold: System");
        break;
      case Command::Inventory:
        image(Inventory,b.center,d*256.f/220.f);
        left(b.center,b.radius,"Inventory","Hold: Character");
        break;
      case Command::Jump:
        image(Jump,b.center,d/0.97f,swimming ? 0.4f : 1.f);
        below(b.center,b.radius,"Jump","");
        break;
      case Command::Weapon:
        image(armed ? Sheathe : Draw,b.center,d/0.97f);
        below(b.center,b.radius,armed ? "Sheathe" : "Draw","Hold: Equipment");
        break;
      case Command::Block:
        image(Block,b.center,d/0.97f);
        below(b.center,b.radius,"Block","");
        break;
      default:
        image(armed && !uiActive ? Attack : Use,b.center,d/0.97f);
        below(b.center,b.radius,uiActive ? "Accept" : (armed ? (classicCombat ? "Action" : "Attack") : "Use"),"");
        break;
      }
    const bool pressed = c==Command::Block ? blockPointer>=0 : buttonPressed(c);
    if(pressed)
      image(Pressed,b.center,d);
    }

  }

void TouchInput::tick() {
  const auto now = Application::tickCount();
  if(tapCaptured) return;
  if(inventoryNavigation && uiActive && !analogMovement) {
    for(size_t i=0;i<4;++i) {
      if(directions[i] && now>=directionRepeat[i]) {
        directionRepeat[i]=now+150;
        command(Command(i),true);
        }
      }
    }
  if(wheelPointer>=0) {
    auto& touch=touches.at(wheelPointer);
    moveWheel(touch,touch.last);
    return;
    }
  for(auto& [id,touch]:touches) {
    if(touch.role==Role::Item) {
      if(!touch.held && !touch.moved && now-touch.pressedAt>=ItemHoldMs) {
        touch.held = true;
        if(onItemTouch)
          onItemTouch(ItemPhase::Hold,touch.last);
        }
      continue;
      }
    // Short button taps fire on release; reserve the joining window for a multi-finger gesture.
    if(multiTap.joining(now)) continue;
    if(touch.pendingWheel && now-touch.pressedAt>=WheelHoldMs) {
      startWheel(id);
      return;
      }
    if(touch.pendingAction && now-touch.pressedAt>=ActionHoldMs) {
      touch.pendingAction = false;
      touch.actionSent = true;
      command(Command::Accept,true);
      }
    if(touch.pendingButton) {
      touch.pendingButton=false;
      touch.actionSent=true;
      command(touch.command,true);
      return;
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
  // Start from the finger's position when the wheel opens, so hold-time drift stays neutral.
  touch.anchor=touch.last;
  touch.wheelMoved=false;
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
  if(debugOverlay)
    update();
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

void TouchInput::adjustValue(Touch& touch, Point pos) {
  const int delta=pos.x-touch.last.x;
  touch.last=pos;
  const int steps=adjustmentDrag.drag(delta,std::max(8,std::min(w(),h())/40));
  if(steps!=0 && uiActive && menuAdjustment) adjustment(steps);
  if(debugOverlay) update();
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
  if(uiActive) {
    const bool vertical=std::abs(moveAxis.y)>=std::abs(moveAxis.x);
    const bool requested[]={vertical && moveAxis.y < -DirectionThreshold,vertical && moveAxis.y > DirectionThreshold,
                            !vertical && moveAxis.x < -DirectionThreshold,!vertical && moveAxis.x > DirectionThreshold};
    for(size_t i=0;i<4;++i) if(!requested[i]) setDirection(Command(i),false);
    for(size_t i=0;i<4;++i) if(requested[i]) setDirection(Command(i),true);
    return;
    }
  if(classicCombat && classicAction && canBlock && !uiActive) {
    using Direction=TouchMovement::Direction;
    const auto direction=TouchMovement::classicDirection(moveAxis.x,moveAxis.y,directions[size_t(Command::Down)]);
    const bool requested[]={direction==Direction::Forward,direction==Direction::Back,
                            direction==Direction::Left,direction==Direction::Right};
    // Release the old chord before pressing the new one: Gothic releases clear combat actions.
    for(size_t i=0;i<4;++i) if(!requested[i]) setDirection(Command(i),false);
    for(size_t i=0;i<4;++i) if(requested[i]) setDirection(Command(i),true);
    return;
    }
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
  directionRepeat[id] = pressed ? Application::tickCount()+350 : 0;
  command(value,pressed);
  }

void TouchInput::setInventoryNavigation(bool enabled) {
  if(inventoryNavigation==enabled) return;
  inventoryNavigation=enabled;
  for(auto& deadline:directionRepeat) deadline=Application::tickCount()+350;
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
  for(auto& [id,touch]:touches)
    if(touch.role==Role::Item && onItemTouch)
      onItemTouch(ItemPhase::Cancel,Point());
  touches.clear();
  adjustmentPointer=-1;
  adjustmentDrag.reset();
  moveAxis = PointF();
  lookDelta = Point();
  movePointer = -1;
  lookPointer = -1;
  blockPointer = -1;
  analogMovement = false;
  classicAction = false;
  }

bool TouchInput::tryGesture(int pointer, const Touch& second) {
  if(!gesturesEnabled || uiActive || touches.size()!=1) return false;
  auto& [firstId,first]=*touches.begin();
  if(first.role!=Role::Move && first.role!=Role::Look && first.role!=Role::Button) return false;
  const auto travel=first.last-first.anchor;
  const float slop=tapSlop();
  if(!TwoFingerSwipe::canPair(second.pressedAt-first.pressedAt,std::hypot(float(travel.x),float(travel.y)),slop))
    return false;
  gestureFirst=firstId;
  gestureSecond=pointer;
  gestureStarted=second.pressedAt;
  gestureFired=false;
  if(first.role==Role::Button && first.actionSent)
    command(first.command,false);
  first.actionSent=false;
  first.pendingAction=false;
  first.pendingButton=false;
  first.pendingWheel=false;
  blockPointer=-1;
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
  const auto elapsed=Application::tickCount()-gestureStarted;
  const int horizontal=TwoFingerSwipe::horizontalDirection(float(a.x),float(a.y),float(b.x),float(b.y),threshold,elapsed);
  if(horizontal!=0) {
    gestureFired=true;
    command(horizontal<0 ? Command::HealthPotion : Command::ManaPotion,true);
    return;
    }
  // Vertical gestures use one side's movement/view meaning; potion swipes may span both sides.
  if((first.anchor.x<w()/2)!=(second.anchor.x<w()/2)) return;
  const int direction=TwoFingerSwipe::direction(float(a.x),float(a.y),float(b.x),float(b.y),threshold,
                                               elapsed);
  if(direction==0) return;
  gestureFired=true;
  if(first.anchor.x>=w()/2) {
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
  return float(std::max(24,std::min(w(),h())/30));
  }

void TouchInput::captureTap(int pointer, const Touch& touch) {
  releaseGesture();
  gestureFirst=-1;
  gestureSecond=-1;
  gestureFired=false;
  tapCaptured=true;
  touches[pointer]=touch;
  for(auto& [id,held]:touches) {
    if(held.role==Role::Button && held.actionSent)
      command(held.command,false);
    held.actionSent=false;
    held.pendingAction=false;
    held.pendingButton=false;
    held.pendingWheel=false;
    held.role=Role::MultiTap;
    }
  blockPointer=-1;
  movePointer=-1;
  lookPointer=-1;
  moveAxis=PointF();
  lookDelta=Point();
  for(size_t i=0;i<4;++i) setDirection(Command(i),false);
  update();
  }
