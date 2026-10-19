#include "directmemory.h"

#include <Tempest/Log>

#include <algorithm>
#include <cstring>

#include "logbudget.h"
#include "resources.h"
#include "utils/gthfont.h"

using namespace Tempest;
using namespace Compatibility;

// The addresses of Gothic2.exe that LeGo calls to make views and texts, see EngineAdr_G2.d
static constexpr uint32_t ZCVIEW__ZCVIEW     = 8017664;
static constexpr uint32_t ZCVIEW__DTOR       = 8017856;
static constexpr uint32_t ZCVIEW__OPEN       = 8023040;
static constexpr uint32_t ZCVIEW__CLOSE      = 8023600;
static constexpr uint32_t ZCVIEW__TOP        = 8021904;
static constexpr uint32_t ZCVIEW__SETSIZE    = 8026016;
static constexpr uint32_t ZCVIEW__MOVE       = 8025824;
static constexpr uint32_t ZCVIEW__INSERTBACK = 8020272;
static constexpr uint32_t ZCVIEW__RENDER     = 8045072;
static constexpr uint32_t ZCVIEW__SETFONTCOLOR = 8034576;
static constexpr uint32_t ZCTEXTURE__LOAD    = 6239904;
static constexpr uint32_t ZCFONTMAN__LOAD    = 7897808;
static constexpr uint32_t ZCFONTMAN__GETFONT = 7898288;
static constexpr uint32_t ZCFONT__GETFONTX   = 7902448;
static constexpr uint32_t ZCFONT__GETFONTY   = 7902432;

/// The virtual size of the screen of Gothic: the positions and the sizes of views and texts are from 0 to 8192
static constexpr float VirtualSize = 8192.f;

/// The size of the screen that the scripts are told about, the sizes of the texts are in the pixels of it
static constexpr int   ScreenW = 800;
static constexpr int   ScreenH = 600;

namespace {
/// The width of a text of Gothic in pixels of the screen of 800x600
int textWidth(const std::string& font, const std::string& txt) {
  try {
    return Resources::font(font,Resources::FontType::Normal,1.f).textSize(txt).w;
    }
  catch(...) {
    return int(txt.size())*10;
    }
  }

int fontHeight(const std::string& font) {
  try {
    return Resources::font(font,Resources::FontType::Normal,1.f).pixelSize();
    }
  catch(...) {
    return 20;
    }
  }
}

/// Views of LeGo (bars, icons, texts of the mods) are made by the functions of the engine and by writing the fields of the
/// object; OpenGothic draws the views that are open (collectOverlay)
void DirectMemory::setupUiFunctions() {
  cpu.register_thiscall(ZCVIEW__ZCVIEW,[this](ptr32_t ptr, int x1, int y1, int x2, int y2, int id) {
    auto* view = mem32.deref<zCView>(ptr);
    if(view==nullptr)
      return;
    view->VIEWID    = id;
    view->ALPHAFUNC = 1;
    view->COLOR     = int32_t(0xFFFFFFFF);
    view->ALPHA     = 255;
    view->FONTCOLOR = int32_t(0xFFFFFFFF);
    view->VPOSX     = x1;
    view->VPOSY     = y1;
    view->VSIZEX    = x2-x1;
    view->VSIZEY    = y2-y1;
    });

  cpu.register_thiscall(ZCVIEW__DTOR,[this](ptr32_t ptr) {
    openViews.erase(std::remove(openViews.begin(),openViews.end(),ptr),openViews.end());
    });

  cpu.register_thiscall(ZCVIEW__OPEN,[this](ptr32_t ptr) {
    if(std::find(openViews.begin(),openViews.end(),ptr)==openViews.end())
      openViews.push_back(ptr);
    });

  cpu.register_thiscall(ZCVIEW__CLOSE,[this](ptr32_t ptr) {
    openViews.erase(std::remove(openViews.begin(),openViews.end(),ptr),openViews.end());
    });

  cpu.register_thiscall(ZCVIEW__TOP,[this](ptr32_t ptr) {
    auto it = std::find(openViews.begin(),openViews.end(),ptr);
    if(it!=openViews.end()) {
      openViews.erase(it);
      openViews.push_back(ptr);
      }
    });

  cpu.register_thiscall(ZCVIEW__SETSIZE,[this](ptr32_t ptr, int x, int y) {
    if(auto* view = mem32.deref<zCView>(ptr)) {
      view->VSIZEX = x;
      view->VSIZEY = y;
      }
    });

  // moves the view by the vector, not to the place
  cpu.register_thiscall(ZCVIEW__MOVE,[this](ptr32_t ptr, int x, int y) {
    if(auto* view = mem32.deref<zCView>(ptr)) {
      view->VPOSX += x;
      view->VPOSY += y;
      }
    });

  // the texture is an object of the engine, a script reads the name from it (zCObject.objectName)
  cpu.register_thiscall(ZCVIEW__INSERTBACK,[this](ptr32_t ptr, std::string img) {
    auto* view = mem32.deref<zCView>(ptr);
    if(view==nullptr)
      return;
    std::string name;
    for(char c:img)
      name.push_back(char(std::toupper(static_cast<unsigned char>(c))));
    auto it = texObjects.find(name);
    if(it==texObjects.end()) {
      const ptr32_t obj = mem32.alloc(64,"zCTexture");
      if(obj==0)
        return;
      if(auto* str = mem32.deref<zString>(obj+16))
        memAssignString(*str,name);
      it = texObjects.emplace(name,obj).first;
      texNames[obj] = name;
      }
    view->BACKTEX = int32_t(it->second);
    });

  cpu.register_thiscall(ZCVIEW__RENDER,[](ptr32_t) {
    // the views are drawn by OpenGothic
    });

  cpu.register_thiscall(ZCVIEW__SETFONTCOLOR,[this](ptr32_t ptr, int color) {
    if(auto* view = mem32.deref<zCView>(ptr))
      view->FONTCOLOR = color;
    });

  // A texture that a script loads (the walls of the houses of CoM): there is no object of the engine behind it
  cpu.register_stdcall(ZCTEXTURE__LOAD,[](std::string img, int flag) {
    (void)img; (void)flag;
    return 0;
    });
  }

/// The fonts: a script asks for a handle and for the object, the object is the name of the font
void DirectMemory::setupFontFunctions() {
  cpu.register_thiscall(ZCFONTMAN__LOAD,[this](ptr32_t, std::string font) -> int32_t {
    std::string name;
    for(char c:font)
      name.push_back(char(std::toupper(static_cast<unsigned char>(c))));
    for(size_t i=0; i<fonts.size(); ++i)
      if(fonts[i].name==name)
        return int32_t(i+1);
    FontInfo f;
    f.name   = name;
    f.object = mem32.alloc(64,"zCFont");
    fonts.push_back(f);
    fontByPtr[f.object] = uint32_t(fonts.size()-1);
    return int32_t(fonts.size());
    });

  cpu.register_thiscall(ZCFONTMAN__GETFONT,[this](ptr32_t, int handle) -> int32_t {
    if(handle<=0 || size_t(handle)>fonts.size())
      return 0;
    return int32_t(fonts[size_t(handle-1)].object);
    });

  // the width of a text and the height of a line in the pixels of the screen
  cpu.register_thiscall(ZCFONT__GETFONTX,[this](ptr32_t self, std::string txt) -> int32_t {
    auto it = fontByPtr.find(self);
    if(it==fontByPtr.end())
      return 0;
    return int32_t(textWidth(fonts[it->second].name,txt));
    });

  cpu.register_thiscall(ZCFONT__GETFONTY,[this](ptr32_t self) -> int32_t {
    auto it = fontByPtr.find(self);
    if(it==fontByPtr.end())
      return 0;
    return int32_t(fontHeight(fonts[it->second].name));
    });
  }

/// Every frame: the size of the screen, the texts of the screen that are shown for a time
void DirectMemory::tickUi(uint64_t dt) {
  if(auto* vScreen = mem32.deref<zCView>(memGame._ZCSESSION_VIEWPORT)) {
    vScreen->PSIZEX = ScreenW;
    vScreen->PSIZEY = ScreenH;
    }

  // the texts that have a time (Print_Ext with a time, PrintS) are removed from the list when it is over
  auto tickList = [this,dt](ptr32_t viewPtr) {
    auto* view = mem32.deref<zCView>(viewPtr);
    if(view==nullptr)
      return;
    ptr32_t prev = 0;
    ptr32_t node = view->TEXTLINES_NEXT;
    for(int guard=0; node!=0 && guard<4096; ++guard) {
      auto* l = mem32.deref<zCList>(node);
      if(l==nullptr)
        break;
      const ptr32_t next = l->next;
      bool          drop = false;
      if(auto* t = mem32.deref<zCViewText>(l->data)) {
        if(t->timed!=0) {
          float timer = 0;
          std::memcpy(&timer,&t->timer,sizeof(timer));
          timer -= float(dt);
          std::memcpy(&t->timer,&timer,sizeof(timer));
          drop = timer<=0.f;
          }
        }
      if(drop) {
        if(prev==0) {
          view->TEXTLINES_NEXT = next;
          } else {
          if(auto* p = mem32.deref<zCList>(prev))
            p->next = next;
          }
        } else {
        prev = node;
        }
      node = next;
      }
    };

  tickList(memGame.ARRAY_VIEW[0]);
  for(auto v:openViews)
    tickList(v);
  }

namespace {
uint32_t argb(int32_t c) {
  return uint32_t(c);
  }
}

void DirectMemory::collectOverlay(std::vector<GameScript::DmaOverlayItem>& out) {
  auto texts = [&](ptr32_t listHead, float vx, float vy, float vw, float vh, int32_t fontColor) {
    ptr32_t node = listHead;
    for(int guard=0; node!=0 && guard<4096; ++guard) {
      const auto* l = mem32.deref<const zCList>(node);
      if(l==nullptr)
        break;
      node = l->next;
      const auto* t = mem32.deref<const zCViewText>(l->data);
      if(t==nullptr)
        continue;

      std::string txt;
      memFromString(txt,t->text);
      if(txt.empty())
        continue;

      GameScript::DmaOverlayItem it;
      it.kind  = GameScript::DmaOverlayItem::Text;
      it.x     = vx + float(t->posx)*vw/VirtualSize;
      it.y     = vy + float(t->posy)*vh/VirtualSize;
      it.name  = txt;
      auto f   = fontByPtr.find(ptr32_t(t->font));
      it.font  = f==fontByPtr.end() ? std::string() : fonts[f->second].name;
      it.color = argb(t->colored!=0 ? t->color : fontColor);
      out.push_back(std::move(it));
      }
    };

  // the views, the last one is in front
  for(auto vp:openViews) {
    const auto* v = mem32.deref<const zCView>(vp);
    if(v==nullptr)
      continue;
    const float x = float(v->VPOSX), y = float(v->VPOSY), w = float(v->VSIZEX), h = float(v->VSIZEY);

    auto tex = texNames.find(ptr32_t(v->BACKTEX));
    if(tex!=texNames.end() && w>0 && h>0) {
      GameScript::DmaOverlayItem it;
      it.kind  = GameScript::DmaOverlayItem::Image;
      it.x     = x; it.y = y; it.w = w; it.h = h;
      it.name  = tex->second;
      // the alpha of the view is the alpha of the color (ViewPtr_SetColor)
      it.color = (argb(v->COLOR) & 0x00FFFFFFu) | (uint32_t(std::clamp(v->ALPHA,0,255))<<24);
      out.push_back(std::move(it));
      }
    texts(v->TEXTLINES_NEXT,x,y,w,h,v->FONTCOLOR);
    }

  // what is printed on the screen: PrintS, Print_Ext
  if(const auto* screen = mem32.deref<const zCView>(memGame.ARRAY_VIEW[0]))
    texts(screen->TEXTLINES_NEXT,0,0,VirtualSize,VirtualSize,int32_t(0xFFFFFFFF));
  }
