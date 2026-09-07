#include "inventorymenu.h"
#include "utils/radialinput.h"

#include <Tempest/Painter>
#include <Tempest/SoundEffect>

#include "utils/string_frm.h"
#include "world/objects/npc.h"
#include "world/objects/interactive.h"
#include "world/objects/item.h"
#include "world/world.h"
#include "utils/gthfont.h"
#include "utils/keycodec.h"
#include "gothic.h"
#include "resources.h"
#include "utils/gamepadbindings.h"

using namespace Tempest;

struct InventoryMenu::Page {
  Page()=default;
  Page(const Page&)=delete;
  virtual ~Page()=default;

  size_t                      size() const {
    if(is(nullptr))
      return 0;
    size_t ret = 0;
    auto it = iterator();
    while(it.isValid()) {
      ret++;
      ++it;
      }
    return ret;
    }
  Inventory::Iterator         get(size_t id) const {
    auto it = iterator();
    for(size_t i=0; i<id && it.isValid(); ++i)
      ++it;
    return it;
    }

  virtual bool                is(const Inventory* i) const { return i==nullptr; }
  virtual Inventory::Iterator iterator() const { throw std::runtime_error("index out of range");  }
  };

struct InventoryMenu::InvPage : InventoryMenu::Page {
  InvPage(const Inventory& i):inv(i){}

  bool                is(const Inventory* i) const override { return &inv==i; }
  Inventory::Iterator iterator() const override {
    return inv.iterator(Inventory::T_Inventory);
    }

  const Inventory& inv;
  };

struct InventoryMenu::TradePage : InventoryMenu::Page {
  TradePage(const Inventory& i):inv(i){}

  bool                is(const Inventory* i) const override { return &inv==i; }
  Inventory::Iterator iterator() const override {
    return inv.iterator(Inventory::T_Trade);
    }

  const Inventory& inv;
  };

struct InventoryMenu::RansackPage : InventoryMenu::Page {
  RansackPage(const Inventory& i):inv(i){}

  bool                is(const Inventory* i) const override { return &inv==i; }
  Inventory::Iterator iterator() const override {
    return inv.iterator(Inventory::T_Ransack);
    }

  const Inventory& inv;
  };

InventoryMenu::InventoryMenu(const KeyCodec& key)
  :keycodec(key) {
  slot = Resources::loadTexture("INV_SLOT.TGA");
  selT = Resources::loadTexture("INV_SLOT_HIGHLIGHTED.TGA");
  selU = Resources::loadTexture("INV_SLOT_EQUIPPED.TGA");
  tex  = Resources::loadTexture("INV_BACK.TGA"); // INV_TITEL.TGA

  int invMaxColumns = Gothic::settingsGetI("GAME","invMaxColumns");
  if(invMaxColumns>0)
    columsCount = size_t(invMaxColumns); else
    columsCount = 5;

  setFocusPolicy(NoFocus);
  setCursorShape(CursorShape::Hidden);
  takeTimer.timeout.bind(this,&InventoryMenu::onTakeStuff);
  }

InventoryMenu::~InventoryMenu() {
  }

void InventoryMenu::close() {
  wheelActive=false;
  if(state!=State::Closed) {
    if(state==State::Trade)
      Gothic::inst().emitGlobalSound("TRADE_CLOSE"); else
      Gothic::inst().emitGlobalSound("INV_CLOSE");
    }
  renderer.reset(true);
  takeTimer.stop();
  state  = State::Closed;
  }

void InventoryMenu::open(Npc &pl) {
  if(pl.isDown() || pl.isMonster() || pl.isInAir() || pl.isSlide() || (pl.interactive()!=nullptr))
    return;
  if(pl.bodyStateMasked()==BS_UNCONSCIOUS || pl.bodyStateMasked()==BS_LIE)
    return;
  if(pl.weaponState()!=WeaponState::NoWeapon) {
    pl.stopAnim("");
    pl.closeWeapon(false);
    }
  state  = State::Equip;
  player = &pl;
  trader = nullptr;
  chest  = nullptr;
  page   = 0;
  pagePl .reset(new InvPage  (pl.inventory()));
  pageOth.reset();
  adjustScroll();
  update();

  Gothic::inst().emitGlobalSound("INV_OPEN");
  //Gothic::inst().emitGlobalSound("INV_CHANGE");
  }

void InventoryMenu::trade(Npc &pl, Npc &tr) {
  if(pl.isDown())
    return;
  state  = State::Trade;
  player = &pl;
  trader = &tr;
  chest  = nullptr;
  page   = 0;
  pagePl .reset(new InvPage  (pl.inventory()));
  pageOth.reset(new TradePage(tr.inventory()));
  adjustScroll();
  update();
  Gothic::inst().emitGlobalSound("TRADE_OPEN");
  }

bool InventoryMenu::ransack(Npc &pl, Npc &tr) {
  if(pl.isDown())
    return false;
  auto it = tr.inventory().iterator(Inventory::T_Ransack);
  if(!it.isValid())
    return false;
  state  = State::Ransack;
  player = &pl;
  trader = &tr;
  chest  = nullptr;
  page   = 0;
  pagePl .reset(new InvPage    (pl.inventory()));
  pageOth.reset(new RansackPage(tr.inventory()));
  adjustScroll();
  update();
  Gothic::inst().emitGlobalSound("INV_OPEN");
  return true;
  }

void InventoryMenu::open(Npc &pl, Interactive &ch) {
  if(pl.isDown())
    return;
  const bool needToPicklock = ch.needToLockpick(pl);
  if(!pl.setInteraction(&ch))
    return;

  if(needToPicklock && !ch.isCracked()) {
    state = State::LockPicking;
    } else {
    state = State::Chest;
    }

  player = &pl;
  trader = nullptr;
  chest  = &ch;
  page   = 0;
  pagePl .reset(new InvPage(pl.inventory()));
  pageOth.reset(new InvPage(ch.inventory()));
  adjustScroll();
  update();
  }

InventoryMenu::State InventoryMenu::isOpen() const {
  return state;
  }

bool InventoryMenu::isActive() const {
  return state!=State::Closed;
  }

void InventoryMenu::onWorldChanged() {
  close();
  player = nullptr;
  trader = nullptr;
  chest  = nullptr;
  }

void InventoryMenu::tick(uint64_t /*dt*/) {
  if(player!=nullptr && (player->isDown() || player->bodyStateMasked()==BS_UNCONSCIOUS)) {
    close();
    return;
    }

  if(state==State::LockPicking) {
    if(chest->isCracked()) {
      state = State::Chest;
      return;
      }
    }

  if(state==State::Ransack) {
    if(trader==nullptr){
      close();
      return;
      }

    if(!trader->isDown()) {
      close();
      return;
      }

    auto it = trader->inventory().iterator(Inventory::T_Ransack);
    if(!it.isValid())
      close();
    }

  if(state==State::Closed) {
    if(player!=nullptr){
      if(!player->setInteraction(nullptr))
         return;
      player = nullptr;
      chest  = nullptr;
      }

    page = 0;
    renderer.reset();
    pagePl .reset();
    pageOth.reset();
    update();
    }
  }

void InventoryMenu::processMove(KeyEvent& e) {
  auto key = keycodec.tr(e);
  if(key==KeyCodec::Forward)
    moveUp();
  else if(key==KeyCodec::Back)
    moveDown();
  else if(key==KeyCodec::Left || key==KeyCodec::RotateL)
    moveLeft(true);
  else if(key==KeyCodec::Right || key==KeyCodec::RotateR)
    moveRight(true);
  }

void InventoryMenu::moveLeft(bool usePage) {
  auto& sel = activePageSel();

  if(usePage && sel.sel%columsCount==0 && page>0)
    page--;
  else if(sel.sel>0)
    sel.sel--;
  }

void InventoryMenu::moveRight(bool usePage) {
  auto&        pg     = activePage();
  auto&        sel    = activePageSel();
  const size_t pCount = pagesCount();

  if(usePage && ((sel.sel+1u)%columsCount==0 || sel.sel+1u==pg.size() || pg.size()==0) && page+1u<pCount)
    page++;
  else if(sel.sel+1<pg.size())
    sel.sel++;
  }

void InventoryMenu::moveUp() {
  auto& sel = activePageSel();

  if(sel.sel>=columsCount)
    sel.sel -= columsCount;
  else
    moveLeft(false);
  }

void InventoryMenu::moveDown() {
  auto& pg  = activePage();
  auto& sel = activePageSel();

  if(sel.sel+columsCount<pg.size())
    sel.sel += columsCount;
  else
    moveRight(false);
  }

void InventoryMenu::keyDownEvent(KeyEvent &e) {
  if(state==State::Closed || state==State::LockPicking){
    e.ignore();
    return;
    }

  processMove(e);

  if(keycodec.tr(e)==KeyCodec::Jump) {
    lootMode = LootMode::Stack;
    takeTimer.start(200);
    onTakeStuff();
    }
  else if (keycodec.tr(e)==KeyCodec::ActionGeneric || e.key==KeyEvent::K_Return) {
    onItemAction(Item::NSLOT);
    }
  else if((KeyEvent::K_3<=e.key && e.key<=KeyEvent::K_9) || e.key==KeyEvent::K_0) {
    uint8_t slot = 10;
    if((KeyEvent::K_3<=e.key && e.key<=KeyEvent::K_9))
      slot = uint8_t(e.key-KeyEvent::K_0);
    onItemAction(slot);
    }
  else if(e.key==KeyEvent::K_ESCAPE || keycodec.tr(e)==KeyCodec::Inventory){
    close();
    }
  else if(e.key==KeyEvent::K_Space) {
    lootMode = LootMode::Normal;
    takeTimer.start(200);
    onTakeStuff();
    }
  else if(e.key==KeyEvent::K_Z) {
    lootMode = LootMode::Ten;
    takeTimer.start(200);
    onTakeStuff();
    }
  else if(e.key==KeyEvent::K_X) {
    lootMode = LootMode::Hundred;
    takeTimer.start(200);
    onTakeStuff();
    }

  adjustScroll();
  update();
  }

void InventoryMenu::keyRepeatEvent(KeyEvent& e) {
  if(state==State::LockPicking || state==State::Closed)
    return;
  processMove(e);
  adjustScroll();
  update();
  }

void InventoryMenu::keyUpEvent(KeyEvent&) {
  takeTimer.stop();
  lootMode = LootMode::Normal;
  }

void InventoryMenu::mouseDownEvent(MouseEvent &e) {
  if(player==nullptr || state==State::Closed) {
    e.ignore();
    return;
    }

  if(state==State::LockPicking)
    return;

  if (e.button==MouseEvent::ButtonLeft)
    onItemAction(Item::NSLOT);
  else if (e.button==MouseEvent::ButtonRight)
    close();

  adjustScroll();
  }

void InventoryMenu::mouseUpEvent(MouseEvent&) {
  takeTimer.stop();
  takeCount=0;
  }

void InventoryMenu::mouseWheelEvent(MouseEvent &e) {
  if(state==State::Closed) {
    e.ignore();
    return;
    }

  if(state==State::LockPicking)
    return;

  scrollDelta += e.delta;
  if(scrollDelta>0) {
    for(int i=0;i<scrollDelta/120;++i)
      moveUp();
    scrollDelta %= 120;
    } else {
    for(int i=0;i<-scrollDelta/120;++i)
      moveDown();
    scrollDelta %= 120;
    }
  adjustScroll();
  }

size_t InventoryMenu::rowsCount() const {
  const int reserved = hasSideInfo() ? 0 : infoHeight();
  return size_t(std::max(1,(h()-gridTop()-reserved-20)/std::max(1,slotSize().h)));
  }

void InventoryMenu::paintEvent(PaintEvent &e) {
  if(player==nullptr || state==State::Closed)
    return;
  renderer.reset();

  Painter p(e);
  drawAll(p,*player,DrawPass::Back);
  }

void InventoryMenu::paintNumOverlay(PaintEvent& e) {
  if(player==nullptr || state==State::Closed)
    return;

  Painter p(e);
  drawAll(p,*player,DrawPass::Front);
  }

Size InventoryMenu::slotSize() const {
  const float scale = Gothic::interfaceScale(this);
  const float cell  = float(Gothic::options().inventoryCellSize);
  return Size(int(cell*scale),int(cell*scale));
  }

int InventoryMenu::infoHeight() const {
  const float scale = Gothic::interfaceScale(this);
  return (Item::MAX_UI_ROWS+2)*int(float(Resources::font(scale).pixelSize()))+int(scale*10)/*padding bottom*/;
  }

bool InventoryMenu::hasSideInfo() const {
#if defined(__ANDROID__)
  const float scale = Gothic::interfaceScale(this);
  const int gap = int(24*scale);
  const int centerWidth = w()-2*(43+int(columsCount)*slotSize().w+gap);
  return centerWidth>=int(360*scale) && h()>=2*infoHeight()+60;
#else
  return false;
#endif
  }

int InventoryMenu::headerTop() const {
  if(hasSideInfo())
    return int(16*Gothic::interfaceScale(this));
  return 70;
  }

int InventoryMenu::gridTop() const {
  if(hasSideInfo())
    return headerTop()+int(42*Gothic::interfaceScale(this));
  return 30+34+70;
  }

Rect InventoryMenu::infoRect() const {
  const float scale = Gothic::interfaceScale(this);
  const int dh = infoHeight();
  if(hasSideInfo()) {
    const int gap = int(24*scale);
    const int x = 43+int(columsCount)*slotSize().w+gap;
    return Rect(x,h()-dh-20,w()-2*x,dh);
    }
  const int dw = std::min(w(),int(720*scale));
  return Rect((w()-dw)/2,h()-dh-20,dw,dh);
  }

size_t InventoryMenu::pagesCount() const {
  if(state==State::Chest || state==State::Trade)
    return 2;
  return 1;
  }

const InventoryMenu::Page &InventoryMenu::activePage() {
  if(pageOth!=nullptr)
    return *(page==0 ? pageOth : pagePl);
  if(pagePl)
    return *pagePl;

  static Page n;
  return n;
  }

InventoryMenu::PageLocal &InventoryMenu::activePageSel() {
  if(pageOth!=nullptr)
    return (page==0 ? pageLocal[0] : pageLocal[1]);
  return pageLocal[1];
  }
  
void InventoryMenu::onItemAction(uint8_t slotHint) {
  auto& page = activePage();
  auto& sel  = activePageSel();

  auto it = page.get(sel.sel);
  if(!it.isValid())
    return;

  if(state==State::Equip) {
    const size_t clsId = it->clsId();
    if(it.isEquipped() && slotHint==Item::NSLOT) {
      player->unequipItem(clsId);
      } else {
      player->useItem(clsId,slotHint,false);
      auto it2 = page.get(sel.sel);
      if((!it2.isValid() || it2->clsId()!=clsId) && sel.sel>0)
        --sel.sel;
      }
    }
  else if(state==State::Chest || state==State::Trade || state==State::Ransack) {
    lootMode = LootMode::Normal;
    takeTimer.start(200);
    onTakeStuff();
    }
  }

void InventoryMenu::onTakeStuff() { 
  size_t itemCount = 0;
  auto& page = activePage();
  auto& sel  = activePageSel();
  if(sel.sel >= page.size())
    return;
  auto it = page.get(sel.sel);
  if(lootMode==LootMode::Normal) {
    ++takeCount;
    itemCount = uint32_t(std::pow(10,takeCount / 10));
    if(it.count() <= itemCount) {
      itemCount = uint32_t(it.count());
      takeCount = 0;
      }
    }
  else if(lootMode==LootMode::Stack) {
    itemCount = it.count();
    }
  else if(lootMode==LootMode::Ten) {
    itemCount = 10;
    }
  else if(lootMode==LootMode::Hundred) {
    itemCount = 100;
    }

  if(it.count() < itemCount) {
    itemCount = it.count();
    }

  if(state==State::Chest) {
    if(page.is(&player->inventory())) {
      player->moveItem(it->clsId(),*chest,itemCount);
      } else {
      player->addItem (it->clsId(),*chest,itemCount);
      }
    }
  else if(state==State::Trade) {
    if(page.is(&player->inventory())) {
      player->sellItem(it->clsId(),*trader,itemCount);
      } else {
      player->buyItem (it->clsId(),*trader,itemCount);
      }
    }
  else if(state==State::Ransack) {
    if(page.is(&trader->inventory())) {
      player->addItem(it->clsId(),*trader,itemCount);
      }
    }
  else if(state==State::Equip) {
    player->dropItem(it->clsId(),itemCount);
    }
  adjustScroll();
  }

void InventoryMenu::adjustScroll() {
  auto& page = activePage();
  auto& sel  = activePageSel();
  sel.sel = std::min(sel.sel, std::max<size_t>(page.size(),1)-1);
  while(sel.sel<sel.scroll*columsCount) {
    if(sel.scroll<=1){
      sel.scroll=0;
      return;
      }
    sel.scroll-=1;
    }

  const size_t hcount=rowsCount();
  while(sel.sel>=(sel.scroll+hcount)*columsCount) {
    sel.scroll+=1;
    }
  }

void InventoryMenu::drawAll(Painter &p, Npc &player, DrawPass pass) {
  if(wheelActive) { drawWheel(p,pass); return; }
  const int padd = 43;

  const int iy = gridTop();

  if(state==State::LockPicking)
    return;

  const int wcount = int(columsCount);
  const int hcount = int(rowsCount());

  if(chest!=nullptr){
    if(pass==DrawPass::Back)
      drawHeader(p,chest->displayName(),padd,headerTop());
    drawItems(p,pass,*pageOth,pageLocal[0],padd,iy,wcount,hcount);
    }

  if(trader!=nullptr) {
    if(pass==DrawPass::Back)
      drawHeader(p,trader->displayName(),padd,headerTop());
    drawItems(p,pass,*pageOth,pageLocal[0],padd,iy,wcount,hcount);
    }

  if(state!=State::Ransack) {
    if(pass==DrawPass::Back)
      drawGold (p,player,w()-padd-2*slotSize().w,headerTop());
    drawItems(p,pass,*pagePl,pageLocal[1],w()-padd-wcount*slotSize().w,iy,wcount,hcount);
    }

  if(pass==DrawPass::Back)
    drawInfo(p);
  }

void InventoryMenu::drawItems(Painter &p, DrawPass pass,
                              const Page &inv, const PageLocal& sel, int x0, int y, int wcount, int hcount) {
  if(state==State::LockPicking)
    return;

  if(tex!=nullptr && pass==DrawPass::Back) {
    p.setBrush(*tex);
    p.drawRect(x0,y,slotSize().w*wcount,slotSize().h*hcount,
               0,0,tex->w(),tex->h());
    }

  auto   it = inv.iterator();
  size_t id = 0;
  for(size_t i=0; it.isValid() && i<sel.scroll*size_t(wcount); ++i) {
    ++it;
    ++id;
    }
  for(int i=0;i<hcount;++i) {
    for(int r=0;r<wcount;++r) {
      const int x = x0 + r*slotSize().w;
      if(pass==DrawPass::Back) {
        p.setBrush(*slot);
        p.drawRect(x,y,slotSize().w,slotSize().h,
                   0,0,slot->w(),slot->h());
        }
      if(it.isValid()) {
        drawSlot(p,pass, it,inv,sel, x,y, id);
        ++it;
        ++id;
        }
      }
    y += slotSize().h;
    }
  }

void InventoryMenu::drawSlot(Painter &p, DrawPass pass, const Inventory::Iterator &it,
                             const Page& page, const PageLocal &sel,
                             int x, int y, size_t id) {
  if(!slot)
    return;

  auto& active = activePage();
  const float scale = Gothic::interfaceScale(this);

  if(pass==DrawPass::Back) {
    if((!it.isValid() && id==0) || (id==sel.sel && &page==&active)){
      p.setBrush(*selT);
      p.drawRect(x,y,slotSize().w,slotSize().h,
                 0,0,selT->w(),selT->h());
      }

    if(it.isEquipped() && selU!=nullptr) {
      p.setBrush(*selU);
      p.drawRect(x,y,slotSize().w,slotSize().h,
                 0,0,selU->w(),selU->h());
      }

    const int dsz = (id==sel.sel ? 5 : 0);
    renderer.drawItem(x-dsz, y-dsz, slotSize().w+2*dsz, slotSize().h+2*dsz, *it);
    } else {
    auto fnt = Resources::font(scale);

    if(it.count()>1) {
      string_frm vint(int(it.count()));
      auto sz = fnt.textSize(vint);
      fnt.drawText(p,x+slotSize().w-sz.w-10,
                   y+slotSize().h-10,
                   vint);
      }

    if(it.slot()!=Item::NSLOT) {
      fnt = Resources::font(Resources::FontType::Red, scale);

      string_frm vint(int(it.slot()));
      auto sz = fnt.textSize(vint);
      fnt.drawText(p,x+10,
                   y+slotSize().h/2+sz.h/2,
                   vint);
      }
    }
  }

void InventoryMenu::drawGold(Painter &p, Npc &player, int x, int y) {
  if(!slot)
    return;
  auto           w    = Gothic::inst().world();
  auto           txt  = w ? w->script().currencyName() : "";
  const size_t   gold = player.inventory().goldCount();
  if(txt.empty())
    txt="Gold";

  string_frm vint(txt," : ",int(gold));
  drawHeader(p,vint,x,y);
  }

void InventoryMenu::drawHeader(Painter &p, std::string_view title, int x, int y) {
  const float scale = Gothic::interfaceScale(this);
  auto&       fnt   = Resources::font(scale);

  const int   tw    = fnt.textSize(title).w;
  const int   th    = fnt.textSize(title).h;
  const int   padd  = int(8*scale);
  const int   dw    = std::max(slotSize().w*2, tw+padd*2);
  const int   dh    = int(34*scale);

  if(tex) {
    p.setBrush(*tex);
    p.drawRect(x,y,dw,dh, 0,0,tex->w(),tex->h());
    }
  if(slot) {
    p.setBrush(*slot);
    p.drawRect(x,y,dw,dh, 0,0,slot->w(),slot->h());
    }

  fnt.drawText(p,x+(dw-tw)/2,y+dh/2+th/2,title);
  }

void InventoryMenu::drawInfo(Painter &p) {
  const float scale = Gothic::interfaceScale(this);
  const auto  rect  = infoRect();
  const int   dw    = rect.w;
  const int   dh    = rect.h;
  const int   x     = rect.x;
  const int   y     = rect.y;

  auto& pg  = activePage();
  auto& sel = activePageSel();

  auto it = pg.get(sel.sel);
  if(!it.isValid())
    return;

  auto& r = *pg.get(sel.sel);
  if(tex) {
    p.setBrush(*tex);
    p.drawRect(x,y,dw,dh,
               0,0,tex->w(),tex->h());
    }

  auto& fnt = Resources::font(scale);
  auto  desc = r.description();
  int   tw   = fnt.textSize(desc).w;

  fnt.drawText(p,x+(dw-tw)/2,y+int(fnt.pixelSize()),desc);

  for(size_t i=0;i<Item::MAX_UI_ROWS;++i){
    auto    txt = r.uiText(i);
    int32_t val = r.uiValue(i);

    if(txt.empty())
      continue;

    if(i+1==Item::MAX_UI_ROWS && state==State::Trade && player!=nullptr && pg.is(&player->inventory())){
      val = r.sellCost();
      }

    string_frm vint(val);
    int tw = fnt.textSize(vint).w;

    fnt.drawText(p, x+20,  y+int(i+2)*fnt.pixelSize(), txt);
    if(val!=0)
      fnt.drawText(p,x+dw-tw-20,y+int(i+2)*fnt.pixelSize(),vint);
    }

  const int sz = dh;
  if(hasSideInfo())
    renderer.drawItem(x+(dw-sz)/2,y-sz-20,sz,sz,r); else
    renderer.drawItem(x+dw-sz-sz/2,y,sz,sz,r);
  }

void InventoryMenu::draw(Tempest::Encoder<CommandBuffer>& cmd) {
  renderer.draw(cmd);
  }

void InventoryMenu::controllerAction(int action) {
  using A=GamepadBindings::Action;
  if(player==nullptr || state==State::Closed || state==State::LockPicking) return;
  switch(A(action)) {
    case A::Back: close(); return;
    case A::Up: moveUp(); break;
    case A::Down: moveDown(); break;
    case A::Left: moveLeft(false); break;
    case A::Right: moveRight(false); break;
    case A::LeftPanel: if(pagesCount()==2) page=0; break;
    case A::RightPanel: if(pagesCount()==2) page=1; break;
    case A::Accept: onItemAction(Item::NSLOT); takeTimer.stop(); takeCount=0; break;
    case A::TakeStack:
      if(state==State::Chest || state==State::Trade || state==State::Ransack) {
        lootMode=LootMode::Stack; onTakeStuff(); lootMode=LootMode::Normal;
        }
      break;
    case A::Drop: {
      if(!activePage().is(&player->inventory())) break;
      auto it=activePage().get(activePageSel().sel);
      if(it.isValid()) player->dropItem(it->clsId(),1);
      break;
      }
    default:
      if(A(action)>=A::Spell3 && A(action)<=A::Spell10 && state==State::Equip) {
        auto it=activePage().get(activePageSel().sel);
        if(it.isValid() && it->isSpellOrRune()) onItemAction(uint8_t(action-int(A::Spell3)+3));
        }
      break;
    }
  adjustScroll(); update();
  }

void InventoryMenu::openWheel(Npc& pl, bool characterMenu) {
  if(pl.isDown() || pl.isMonster() || pl.interactive()!=nullptr) return;
  if(!characterMenu && (!pl.canSwitchWeapon() || !pl.isAiQueueEmpty())) return;
  state=State::Equip; player=&pl; trader=nullptr; chest=nullptr; page=0;
  pagePl.reset(new InvPage(pl.inventory())); pageOth.reset();
  wheelItems.clear();
  for(auto it=pl.inventory().iterator(Inventory::T_Inventory);!characterMenu && it.isValid();++it) {
    if(!it->checkCond(pl)) continue;
    if((it->mainFlag()&(ITM_CAT_NF|ITM_CAT_FF))!=0 || (it->isSpellOrRune() && it.slot()!=Item::NSLOT))
      wheelItems.push_back(it->clsId());
    }
  wheelActive=true; wheelPageId=0; wheelSelected=-1; wheelCentered=true;
  wheelCharacter=characterMenu; wheelTouch=false;
  update();
  }

size_t InventoryMenu::wheelPageSize() const {
  return wheelTouch && wheelItems.size()>8 ? 6 : 8;
  }

size_t InventoryMenu::wheelSectorCount() const {
  // Paged wheels keep their navigation arrows in the same two sectors.
  if(!wheelTouch || wheelPageSize()==6) return 8;
  return wheelCharacter ? 2 : std::min<size_t>(8,wheelItems.size());
  }

InventoryMenu::WheelLayout InventoryMenu::wheelLayout() const {
  const float scale=Gothic::interfaceScale(this);
  const int cell=int(64*scale);
  const float radius=std::min(float(h())*0.30f,float(w())*0.20f);
  Point center(w()/2,h()/2);
  if(wheelTouch) {
    const float fontScale=std::min(scale,float(std::min(w(),h()))/720.f);
    const auto layout=RadialInput::touchLayout(w(),h(),scale,
                                              int(wheelSectorCount()),Resources::font(fontScale).pixelSize());
    return {Point(int(std::lround(layout.x)),int(std::lround(layout.y))),layout.cell,layout.radius,layout.outer,layout.footer};
    }
  return {center,cell,radius,radius+float(cell),0};
  }

void InventoryMenu::beginTouchWheel() {
  wheelTouch=true;
  wheelHoverPage=0;
  wheelPageArmed=true;
  update();
  }

void InventoryMenu::touchWheelMove(Point pos, uint64_t now) {
  if(!wheelActive || !wheelTouch) return;
  const auto layout=wheelLayout();
  const auto delta=pos-layout.center;
  const int selected=RadialInput::sector(float(delta.x),float(delta.y),layout.radius*0.52f,layout.outer,int(wheelSectorCount()));
  if(selected<0) {
    wheelSelected=-1;
    wheelHoverPage=0;
    if(std::hypot(float(delta.x),float(delta.y))<layout.radius*0.52f) {
      wheelCentered=true;
      wheelPageArmed=true;
      }
    update();
    return;
    }
  if(!wheelCharacter && wheelPageSize()==6 && selected>=6) {
    wheelSelected=-1;
    const int direction=selected==6 ? -1 : 1;
    if(wheelHoverPage!=direction) {
      wheelHoverPage=direction;
      wheelHoverSince=now;
      }
    if(wheelPageArmed && now-wheelHoverSince>=500) {
      wheelPage(direction);
      wheelPageArmed=false;
      }
    update();
    return;
    }
  wheelHoverPage=0;
  if(!wheelCentered) return;
  wheelSelected=selected;
  if(wheelCharacter) {
    if(selected!=0 && selected!=1) wheelSelected=-1;
    }
  else if(wheelPageId*wheelPageSize()+size_t(selected)>=wheelItems.size()) {
    wheelSelected=-1;
    }
  update();
  }

void InventoryMenu::wheelMove(float x,float y) {
  if(x*x+y*y<0.25f) { wheelSelected=-1; wheelCentered=true; update(); return; }
  if(!wheelCentered) return;
  float angle=std::atan2(x,-y);
  if(angle<0) angle+=2.f*float(M_PI);
  wheelSelected=int(std::floor(angle/(float(M_PI)/4.f)+0.5f))%8;
  if(wheelPageId*wheelPageSize()+size_t(wheelSelected)>=wheelItems.size()) wheelSelected=-1;
  update();
  }

void InventoryMenu::wheelPage(int direction) {
  const auto count=wheelPageSize();
  const auto pages=std::max<size_t>(1,(wheelItems.size()+count-1)/count);
  wheelPageId=(wheelPageId+pages+(direction<0?pages-1:1))%pages;
  wheelSelected=-1; wheelCentered=false;
  update();
  }

size_t InventoryMenu::wheelSelection() const {
  if(wheelCharacter) return wheelSelected==0 ? 0 : (wheelSelected==(wheelTouch ? 1 : 4) ? 1 : size_t(-1));
  if(wheelSelected<0 || wheelPageId*wheelPageSize()+size_t(wheelSelected)>=wheelItems.size()) return size_t(-1);
  return wheelItems[wheelPageId*wheelPageSize()+size_t(wheelSelected)];
  }

void InventoryMenu::drawWheel(Painter& p,DrawPass pass) {
  if(wheelTouch) { drawTouchWheel(p,pass); return; }
  const float scale=Gothic::interfaceScale(this);
  const auto layout=wheelLayout();
  const auto cell=layout.cell;
  const auto radius=layout.radius;
  const int cx=layout.center.x, cy=layout.center.y;
  auto& font=Resources::font(scale);
  if(pass==DrawPass::Back) {
    p.setBrush(Color(0.04f,0.03f,0.02f,0.85f));
    p.drawRect(cx-int(radius)-cell,cy-int(radius)-cell,2*(int(radius)+cell),2*(int(radius)+cell));
    }
  for(size_t i=0;i<8;++i) {
    const bool pageButton=!wheelCharacter && wheelPageSize()==6 && i>=6;
    if(wheelCharacter && i!=0 && i!=4) continue;
    const auto itemId=wheelPageId*wheelPageSize()+i;
    auto item=!wheelCharacter && !pageButton && itemId<wheelItems.size() ? player->getItem(wheelItems[itemId]) : nullptr;
    if(!wheelCharacter && !pageButton && item==nullptr) continue;
    const float angle=float(i)*float(M_PI)/4.f;
    const int x=cx+int(std::sin(angle)*radius)-cell/2;
    const int y=cy-int(std::cos(angle)*radius)-cell/2;
    if(pass==DrawPass::Back) {
      const bool hovered=pageButton && wheelHoverPage==(i==6 ? -1 : 1);
      const auto texture=int(i)==wheelSelected || hovered ? selT : slot;
      if(texture) { p.setBrush(*texture); p.drawRect(x,y,cell,cell,0,0,texture->w(),texture->h()); }
      if(item) renderer.drawItem(x,y,cell,cell,*item);
      }
    else if(wheelCharacter || pageButton) {
      const auto text=wheelCharacter ? (i==0 ? "Character stats" : "Journal") : (i==6 ? "Previous" : "Next");
      font.drawText(p,x-cell/2,y+cell/2-font.pixelSize()/2,2*cell,2*font.pixelSize(),text,AlignHCenter);
      }
    }
  if(pass==DrawPass::Front) {
    const auto count=wheelPageSize();
    string_frm pageLabel("Equipment ",wheelPageId+1," / ",std::max<size_t>(1,(wheelItems.size()+count-1)/count));
    const int textWidth=int(radius*1.7f);
    font.drawText(p,cx-textWidth/2,cy-font.pixelSize(),textWidth,4*font.pixelSize(),wheelCharacter ? "Character" : pageLabel.c_str(),AlignHCenter);
    auto item=wheelCharacter ? nullptr : player->getItem(wheelSelection());
    const auto name=wheelCharacter ? std::string_view("Center: cancel") : (item ? item->description() : std::string_view("Select equipment"));
    font.drawText(p,cx-textWidth/2,cy+font.pixelSize(),textWidth,3*font.pixelSize(),name,AlignHCenter);
    font.drawText(p,20,h()-3*font.pixelSize(),w()-40,2*font.pixelSize(),wheelHint,AlignHCenter);
    }
  }

void InventoryMenu::drawTouchWheel(Painter& p,DrawPass pass) {
  const auto layout=wheelLayout();
  const float cx=float(layout.center.x), cy=float(layout.center.y);
  const float inner=layout.radius*0.52f;
  const int cell=layout.cell;
  const auto count=wheelSectorCount();
  const float step=count>0 ? 2.f*float(M_PI)/float(count) : 0.f;
  const float scale=std::min(Gothic::interfaceScale(this),float(std::min(w(),h()))/720.f);
  auto& font=Resources::font(scale);
  const int line=font.pixelSize();
  const int pad=std::max(4,int(8*scale));
  const int footerTop=int(cy+layout.outer)+pad;
  const int footerLeft=int(cx-layout.outer);
  const int footerWidth=int(2*layout.outer);
  const Color gold(0.843f,0.761f,0.631f,0.85f);

  p.pushState();
  auto point=[&](float radius,float angle) {
    return Point(int(std::lround(cx+std::sin(angle)*radius)),int(std::lround(cy-std::cos(angle)*radius)));
    };
  auto band=[&](float r0,float r1,float a0,float a1,const Color& color) {
    p.setBrush(color);
    const int segments=std::max(1,int(std::ceil((a1-a0)*24.f)));
    for(int s=0;s<segments;++s) {
      const float a=a0+(a1-a0)*float(s)/float(segments);
      const float b=a0+(a1-a0)*float(s+1)/float(segments);
      const auto p0=point(r0,a), p1=point(r1,a), p2=point(r1,b), p3=point(r0,b);
      p.drawTriangle(p0.x,p0.y,0.f,0.f,p1.x,p1.y,0.f,0.f,p2.x,p2.y,0.f,0.f);
      if(r0>0.f) p.drawTriangle(p0.x,p0.y,0.f,0.f,p2.x,p2.y,0.f,0.f,p3.x,p3.y,0.f,0.f);
      }
    };
  if(pass==DrawPass::Back) {
    band(0.f,layout.outer,0.f,2.f*float(M_PI),Color(0.035f,0.026f,0.015f,0.88f));
    for(size_t i=0;i<count;++i) {
      const bool pageButton=!wheelCharacter && wheelPageSize()==6 && i>=6;
      const bool hovered=pageButton && wheelHoverPage==(i==6 ? -1 : 1);
      if(int(i)==wheelSelected || hovered)
        band(inner,layout.outer,float(i)*step-step/2,float(i)*step+step/2,Color(0.7f,0.47f,0.18f,0.35f));
      }
    band(layout.outer-std::max(1.5f,scale),layout.outer,0.f,2.f*float(M_PI),gold);
    band(inner-1.f,inner,0.f,2.f*float(M_PI),Color(0.843f,0.761f,0.631f,0.4f));
    p.setBrush(gold);
    p.setPen(Pen(Color(0.843f,0.761f,0.631f,0.4f),Painter::Alpha,std::max(1.f,scale)));
    if(count>1) {
      for(size_t i=0;i<count;++i) {
        const float angle=float(i)*step-step/2;
        p.drawLine(point(inner,angle),point(layout.outer,angle));
        }
      }
    p.setBrush(Color(0.035f,0.026f,0.015f,0.8f));
    p.drawRect(footerLeft,footerTop,footerWidth,layout.footer-pad);
    }

  for(size_t i=0;i<count;++i) {
    const bool pageButton=!wheelCharacter && wheelPageSize()==6 && i>=6;
    const auto itemId=wheelPageId*wheelPageSize()+i;
    auto item=!wheelCharacter && !pageButton && itemId<wheelItems.size() ? player->getItem(wheelItems[itemId]) : nullptr;
    const auto at=point(layout.radius,float(i)*step);
    if(pass==DrawPass::Back && item)
      renderer.drawItem(at.x-cell/2,at.y-cell/2,cell,cell,*item);
    if(pass==DrawPass::Front && (wheelCharacter || pageButton)) {
      const auto label=wheelCharacter ? (i==0 ? "Stats" : "Journal") : (i==6 ? "<" : ">");
      font.drawText(p,at.x-cell,at.y+line/2,2*cell,line,label,AlignHCenter);
      }
    }
  if(pass==DrawPass::Front) {
    // Gothic fonts position text by its baseline, not by the top of its box.
    const int centerWidth=int(inner*1.8f);
    font.drawText(p,int(cx)-centerWidth/2,int(cy)+line/2,centerWidth,line,"Cancel",AlignHCenter);
    auto item=wheelCharacter ? nullptr : player->getItem(wheelSelection());
    std::string_view title=wheelCharacter ? "Character" : (wheelItems.empty() ? "No equipment" : "Equipment");
    if(item) title=item->description();
    if(wheelCharacter && wheelSelected>=0) title=wheelSelected==0 ? "Character stats" : "Journal";
    font.drawText(p,footerLeft+pad,footerTop+pad+line,footerWidth-2*pad,2*line,title,AlignHCenter);
    const auto pages=std::max<size_t>(1,(wheelItems.size()+wheelPageSize()-1)/wheelPageSize());
    string_frm paging("Page ",wheelPageId+1," / ",pages," - hold < / >");
    const auto hint=wheelHoverPage!=0 ? "Hold to change page" :
                    (wheelSelected>=0 ? "Release to select" : (pages>1 ? paging.c_str() : "Drag to select"));
    font.drawText(p,footerLeft+pad,footerTop+layout.footer-pad,footerWidth-2*pad,line,hint,AlignHCenter);
    }
  p.popState();
  }
