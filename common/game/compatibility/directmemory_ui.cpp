#include "directmemory.h"

#include <Tempest/Log>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstring>
#include <exception>

#include "logbudget.h"
#include "resources.h"
#include "utils/gthfont.h"
#include "hookaddresses.h"
#include "world/objects/item.h"
#include "world/world.h"

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
static constexpr uint32_t ZRND_D3D__DRAWPOLYSIMPLE = 6597680; // 0x64ac30
static constexpr uint32_t ZRND_D3D__SETALPHABLENDFUNC = 6628880; // 0x652610
static constexpr uint32_t ZRND_D3D__SETRENDERSTATE = 6573808; // 0x644ef0
static constexpr uint32_t OCITEM__RENDER = 7420608; // 0x713ac0
static constexpr uint32_t ZCFONTMAN__LOAD    = 7897808;
static constexpr uint32_t ZCFONTMAN__GETFONT = 7898288;
static constexpr uint32_t ZCFONT__GETFONTX   = 7902448;
static constexpr uint32_t ZCFONT__GETFONTNAME = 7902368;
static constexpr uint32_t ZCFONT__GETFONTY   = 7902432;

/// The virtual size of the screen of Gothic: the positions and the sizes of views and texts are from 0 to 8192
static constexpr float VirtualSize = 8192.f;

namespace {
/// The width of a text of Gothic in pixels of the screen of 800x600
int textWidth(const std::string& font, const std::string& txt, float scale) {
  try {
    return Resources::font(font,Resources::FontType::Normal,scale).textSize(txt).w;
    }
  catch(...) {
    return int(float(txt.size())*10.f*scale);
    }
  }

int fontHeight(const std::string& font, float scale) {
  try {
    return Resources::font(font,Resources::FontType::Normal,scale).pixelSize();
    }
  catch(...) {
    return std::max(1,int(20.f*scale));
    }
  }
}

// CoM's InformationManager hook reads Gothic's private dialog widget. Feed the
// same recipe-selection rules from OpenGothic's real choices instead. Keep the
// mod's CRAFTINGVIEW_SHOW/HIDE for the contents, localization and recipe data.
void DirectMemory::eventDialogChoice(const GameScript::DlgChoice* choice, bool subChoice, int selection) {
  auto* update = vm.find_symbol_by_name("CRAFTINGVIEW_UPDATE");
  auto callbacks = hooks.find(0x660BB0);
  if(update==nullptr || callbacks==hooks.end() ||
     std::find(callbacks->second.begin(),callbacks->second.end(),update->index())==callbacks->second.end())
    return;
  auto* show = vm.find_symbol_by_name("CRAFTINGVIEW_SHOW");
  auto* hide = vm.find_symbol_by_name("CRAFTINGVIEW_HIDE");
  auto* isOpen = vm.find_symbol_by_name("CRAFTINGVIEW_ISOPEN");
  auto* last = vm.find_symbol_by_name("CRAFTINGVIEWLASTCHOICE");
  if(show==nullptr || hide==nullptr || isOpen==nullptr || last==nullptr)
    return;

  try {
    if(choice==nullptr || choice->handle==nullptr) {
      if(isOpen->get_int()!=0)
        vm.call_function(hide);
      craftingChoice.clear();
      last->set_int(-1);
      return;
      }
    const auto key = std::to_string(choice->scriptFn)+(subChoice ? ":sub:" : ":info:")+choice->title;
    if(craftingChoice==key && last->get_int()==selection)
      return;

    int recipe = -1, mode = -1, amount = 1;
    auto integer = [&](std::string_view name) {
      auto* sym = vm.find_symbol_by_name(name);
      return sym!=nullptr && sym->type()==zenkit::DaedalusDataType::INT ? sym->get_int() : -1;
      };
    auto recipeAt = [&](std::string_view name, int index) {
      auto* sym = vm.find_symbol_by_name(name);
      return sym!=nullptr && sym->type()==zenkit::DaedalusDataType::INT && index>=0 && uint32_t(index)<sym->count()
             ? sym->get_int(uint16_t(index)) : -1;
      };
    if(subChoice) {
      for(auto name:{"POTIONALCHEMY_INSTANCE","SCROLL_WRITING_INSTANCE","COOKINGMEALS_INSTANCE"}) {
        recipe = integer(name);
        if(recipe!=-1)
          break;
        }
      if(recipe!=-1) {
        mode = 1;
        auto text = std::string_view(choice->title);
        while(!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
          text.remove_prefix(1);
        const auto parsed = std::from_chars(text.data(),text.data()+text.size(),amount);
        if(parsed.ec!=std::errc() || amount<=0) {
          amount = -1;
          auto* all = vm.find_symbol_by_name("DIALOG_ALL");
          auto* maximum = vm.find_symbol_by_name("PC_CRAFTRECIPEITEM_MAX");
          if(all!=nullptr && maximum!=nullptr) {
            const auto prefix = all->get_string()+" (";
            if(text.starts_with(prefix) && text.ends_with(")")) {
              int max = 0;
              withRecipeTemporary(uint32_t(recipe),"PC_CRAFTRECIPEITEM_MAX",[&]() {
                max = vm.call_function<int>(maximum,recipe);
                });
              if(text==prefix+std::to_string(max)+")")
                amount = max;
              }
            }
          }
        }
      } else {
      const auto nr = choice->handle->nr;
      if(nr>=10000 && nr<10100) { recipe=recipeAt("WEAPONRECIPEINSTANCE",nr-10000); mode=0; }
      if(nr>=10100 && nr<10200) { recipe=recipeAt("POTIONRECIPEINSTANCE",nr-10100); mode=1; }
      if(nr>=10200 && nr<10300) { recipe=recipeAt("MEALRECIPEINSTANCE",nr-10200); mode=1; }
      if(nr>=10300 && nr<10400) { recipe=recipeAt("SCROLLRECIPEINSTANCE",nr-10300); mode=1; }
      }
    if(recipe==-1) {
      if(auto* armor = vm.find_symbol_by_name("CRAFTINGVIEW_CHECKARMORS")) {
        recipe = vm.call_function<int>(armor,std::string_view(choice->title));
        mode = 2;
        }
      }
    if(recipe>0 && amount>=0) {
      withRecipeTemporary(uint32_t(recipe),"CRAFTINGVIEW_SHOW",[&]() {
        vm.call_function(show,recipe,mode,amount);
        });
      } else if(isOpen->get_int()!=0) {
      vm.call_function(hide);
      }
    craftingChoice = key;
    last->set_int(selection);
    }
  catch(const std::exception& e) {
    if(logBudget("crafting-choice"))
      Log::e("CoM crafting selection: ",e.what());
    }
  }

/// Views of LeGo (bars, icons, texts of the mods) are made by the functions of the engine and by writing the fields of the
/// object; OpenGothic draws the views that are open (collectOverlay)
void DirectMemory::setupUiFunctions() {
  if(auto* show = vm.find_symbol_by_name("CRAFTINGVIEW_SHOW")) {
    // Tab switches call SHOW directly, outside eventDialogChoice. They need
    // the same temporary-recipe cleanup as a new recipe selection.
    vm.override_function("CRAFTINGVIEW_SHOW",[this,show](int recipe, int mode, int amount) {
      withRecipeTemporary(uint32_t(recipe),"CRAFTINGVIEW_SHOW",[&]() {
        vm.call_function(show,recipe,mode,amount); // Deliberately run its original body.
        });
      });
    }
  cpu.register_thiscall(ZCVIEW__ZCVIEW,[this](ptr32_t ptr, int x1, int y1, int x2, int y2, int id) {
    auto* view = mem32.deref<zCView>(ptr);
    if(view==nullptr)
      return;
    view->VIEWID    = id;
    view->ALPHAFUNC = 1;
    view->COLOR     = int32_t(0xFFFFFFFF);
    view->ALPHA     = 255;
    view->FONTCOLOR = int32_t(0xFFFFFFFF);
    // a view has the default font as in the engine: LeGo saves its name (GetFontName) and sets it back after the load
    view->FONT      = int32_t(fonts[fontHandle("FONT_OLD_10_WHITE.TGA")-1].object);
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
    view->BACKTEX = int32_t(textureHandle(img));
    });

  cpu.register_thiscall(ZCVIEW__RENDER,[this](ptr32_t ptr) {
    if(collectingSprites && mem32.deref<zCView>(ptr)!=nullptr)
      renderedViews.push_back(ptr);
    });

  cpu.register_thiscall(ZCVIEW__SETFONTCOLOR,[this](ptr32_t ptr, int color) {
    if(auto* view = mem32.deref<zCView>(ptr))
      view->FONTCOLOR = color;
    });

  cpu.register_stdcall(ZCTEXTURE__LOAD,[this](std::string img, int /*flags*/) -> ptr32_t {
    return textureHandle(img);
    });

  // Itm_GetPtr supplies crafting recipes and item previews with an isolated
  // item instance. Keep one per class, without spawning a helper NPC into a
  // nonexistent TOT waypoint or adding items to the player's inventory.
  if(vm.find_symbol_by_name("ITM_GETPTR")!=nullptr) {
    vm.override_function("ITM_GETPTR",[this](int32_t cls) -> int32_t {
      auto* sym = cls>0 ? vm.find_symbol_by_index(uint32_t(cls)) : nullptr;
      if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::INSTANCE)
        return 0;
      auto* parent = sym;
      for(size_t guard=0; parent!=nullptr && parent->type()!=zenkit::DaedalusDataType::CLASS; ++guard) {
        if(guard>=vm.symbols().size())
          return 0;
        parent = vm.find_symbol_by_index(parent->parent());
        }
      if(parent==nullptr || parent->name()!="C_ITEM")
        return 0;
      auto it = previewItems.find(uint32_t(cls));
      if(it==previewItems.end()) {
        auto item = std::make_shared<Item>(gameScript.world(),size_t(cls),Item::T_Inventory);
        it = previewItems.emplace(uint32_t(cls),std::move(item)).first;
        }
      return int32_t(handles.pointerTo(it->second->handlePtr()));
      });
    }
  cpu.register_thiscall(OCITEM__RENDER,[this](ptr32_t ptr, ptr32_t /*world*/, ptr32_t viewPtr, float /*rotation*/) {
    if(!collectingSprites || spriteOverlay.size()>=16384)
      return;
    auto instance = handles.instanceAt(ptr);
    auto item = std::dynamic_pointer_cast<zenkit::IItem>(instance);
    auto* view = mem32.deref<zCView>(viewPtr);
    if(item==nullptr || item->user_ptr==nullptr || view==nullptr || view->VSIZEX<=0 || view->VSIZEY<=0 ||
       std::find(openViews.begin(),openViews.end(),viewPtr)==openViews.end())
      return;
    GameScript::DmaOverlayItem result;
    result.kind = GameScript::DmaOverlayItem::Model;
    result.x = float(view->VPOSX);
    result.y = float(view->VPOSY);
    result.w = float(view->VSIZEX);
    result.h = float(view->VSIZEY);
    result.model = std::move(item);
    spriteOverlay.push_back(std::move(result));
    });

  cpu.register_thiscall(ZRND_D3D__SETALPHABLENDFUNC,[this](ptr32_t, int blend) {
    uiBlend = blend;
    });
  cpu.register_thiscall(ZRND_D3D__SETRENDERSTATE,[](ptr32_t, int state, int /*value*/) {
    // Sprite rendering is submitted to the depth-independent UI pass. Its
    // D3DRS_ZENABLE toggle therefore never changes the world's depth state.
    if(state!=7 && logBudget("ui-render-state"))
      Log::e("LeGo sprite: unsupported D3D render state ",state);
    });
  cpu.register_thiscall(ZRND_D3D__DRAWPOLYSIMPLE,[this](ptr32_t, ptr32_t texture, ptr32_t stream, int count) {
    if(!collectingSprites || count<3 || count>64 || spriteOverlay.size()>16320)
      return;
    const auto name = texNames.find(texture);
    if(name==texNames.end())
      return;
    struct Vertex { float x,y,z,u,v; uint32_t color; };
    static_assert(sizeof(Vertex)==24);
    const auto* data = mem32.derefv(stream,uint32_t(size_t(count)*sizeof(Vertex)));
    if(data==nullptr)
      return;
    Vertex vertices[64];
    std::memcpy(vertices,data,size_t(count)*sizeof(Vertex));
    for(int i=0; i<count; ++i) {
      const auto& v = vertices[i];
      if(!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.u) || !std::isfinite(v.v))
        return;
      // CoM's map and cursor use uniform Sprite_SetColor. Painter does not
      // provide per-vertex colors; report that distinct unsupported contract.
      if(v.color!=vertices[0].color) {
        if(logBudget("ui-vertex-colors"))
          Log::e("LeGo sprite: per-vertex color gradients are not supported");
        return;
        }
      }
    if(uiBlend!=1 && uiBlend!=2 && uiBlend!=3) {
      if(logBudget("ui-blend"))
        Log::e("LeGo sprite: unsupported blend mode ",uiBlend);
      return;
      }
    for(int i=1; i+1<count; ++i) {
      GameScript::DmaOverlayItem item;
      item.kind = GameScript::DmaOverlayItem::Triangle;
      item.name = name->second;
      item.color = vertices[0].color;
      item.blend = uiBlend;
      // Preserve the requested sprite priority independently of the order of
      // PermMem's handle table (the map cursor must stay above the map).
      auto* sprite = vm.find_symbol_by_name("SPRITE_RENDER.S");
      auto* priority = vm.find_symbol_by_name("GCSPRITE.PRIO");
      if(sprite!=nullptr && priority!=nullptr) {
        auto& instance = sprite->get_instance();
        if(auto* memory = dynamic_cast<memory_instance*>(instance.get()))
          item.priority = memory->get_int(*priority,0);
        }
      const int indices[3] = {0,i,i+1};
      for(int j=0; j<3; ++j) {
        const auto& v = vertices[indices[j]];
        item.vertices[j] = {v.x*VirtualSize/float(uiWidth),v.y*VirtualSize/float(uiHeight),v.u,v.v};
        }
      spriteOverlay.push_back(std::move(item));
      }
    });
  }

void DirectMemory::initItemRenderer() {
  auto* list = vm.find_symbol_by_name("_RENDER_LIST");
  auto* world = vm.find_symbol_by_name("_RENDER_WLD");
  auto* callback = vm.find_symbol_by_name("_RENDER_HOOK");
  if(list==nullptr || world==nullptr || callback==nullptr)
    return;
  // In CoM's embedded LeGo these are raw pointers, not PermMem handles.
  // OpenGothic's InventoryRenderer owns the rendering scene; only the list
  // and an opaque renderer token are required by the surrounding scripts.
  if(renderList==0) {
    renderList = mem32.alloc(uint32_t(sizeof(zCList)),"LeGo Render list");
    if(renderList==0)
      return;
    }
  list->set_int(int32_t(renderList));
  if(renderWorld==0) {
    renderWorld = mem32.alloc(4,"LeGo UI renderer token");
    if(renderWorld==0)
      return;
    }
  world->set_int(int32_t(renderWorld));
  auto& callbacks = hooks[HookAdr::GameRender];
  if(std::find(callbacks.begin(),callbacks.end(),callback->index())==callbacks.end())
    callbacks.push_back(callback->index());
  }

ptr32_t DirectMemory::textureHandle(std::string_view texture) {
  std::string name(texture);
  for(auto& c:name)
    c = char(std::toupper(static_cast<unsigned char>(c)));
  if(auto it = texObjects.find(name); it!=texObjects.end())
    return it->second;
  if(Resources::loadTexture(name)==nullptr) {
    if(logBudget(std::string("ui-texture:")+name))
      Log::e("LeGo texture not found: ",name);
    return 0;
    }
  const auto ptr = mem32.alloc(160,"zCTexture of a UI sprite/view");
  if(ptr==0)
    return 0;
  auto* objectName = mem32.deref<zString>(ptr+16);
  if(objectName==nullptr || !memAssignString(*objectName,name)) {
    mem32.free(ptr);
    return 0;
    }
  mem32.writeInt(ptr+4,1); // zCObject.refCtr
  texObjects[name] = ptr;
  texNames[ptr] = std::move(name);
  return ptr;
  }

/// The handle (the index+1) of a font, the font is made when it is asked for the first time
uint32_t DirectMemory::fontHandle(std::string_view font) {
  std::string name;
  for(char c:font)
    name.push_back(char(std::toupper(static_cast<unsigned char>(c))));
  for(size_t i=0; i<fonts.size(); ++i)
    if(fonts[i].name==name)
      return uint32_t(i+1);
  FontInfo f;
  f.name   = name;
  f.object = mem32.alloc(64,"zCFont");
  fonts.push_back(f);
  fontByPtr[f.object] = uint32_t(fonts.size()-1);
  return uint32_t(fonts.size());
  }

/// The fonts: a script asks for a handle and for the object, the object is the name of the font
void DirectMemory::setupFontFunctions() {
  cpu.register_thiscall(ZCFONTMAN__LOAD,[this](ptr32_t, std::string font) -> int32_t {
    return int32_t(fontHandle(font));
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
    return int32_t(textWidth(fonts[it->second].name,txt,std::min(float(uiWidth)/800.f,float(uiHeight)/600.f)));
    });

  // zCFont::GetFontName() returns a zSTRING by value: the script gives the place for it (CALL_RetValIsZString) and takes
  // the text from it afterwards (CALL_RetValAszString). LeGo asks for it when it saves a view: the font has to come back
  // after the load.
  cpu.register_thiscall(ZCFONT__GETFONTNAME,[this](ptr32_t self, ptr32_t out) -> int32_t {
    auto* z = mem32.deref<zString>(out);
    if(z==nullptr)
      return 0;
    auto it = fontByPtr.find(self);
    memAssignString(*z,it==fontByPtr.end() ? std::string_view("FONT_OLD_10_WHITE.TGA") : std::string_view(fonts[it->second].name));
    return int32_t(out);
    });

  cpu.register_thiscall(ZCFONT__GETFONTY,[this](ptr32_t self) -> int32_t {
    auto it = fontByPtr.find(self);
    if(it==fontByPtr.end())
      return 0;
    return int32_t(fontHeight(fonts[it->second].name,std::min(float(uiWidth)/800.f,float(uiHeight)/600.f)));
    });
  }

/// Every frame: the size of the screen, the texts of the screen that are shown for a time
void DirectMemory::tickUi(uint64_t dt) {
  if(auto* vScreen = mem32.deref<zCView>(memGame._ZCSESSION_VIEWPORT)) {
    vScreen->PSIZEX = uiWidth;
    vScreen->PSIZEY = uiHeight;
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

void DirectMemory::collectOverlay(std::vector<GameScript::DmaOverlayItem>& out, int width, int height) {
  const bool resized = uiWidth!=std::max(width,1) || uiHeight!=std::max(height,1);
  uiWidth = std::max(width,1);
  uiHeight = std::max(height,1);
  // CoM's 800x200 crafting panel fits the viewport. Its script computes the
  // scale from the virtual width of the HP bar, not from a UI-only constant.
  const float scale = std::min(float(uiWidth)/800.f,float(uiHeight)/600.f);
  if(auto* bar = mem32.deref<oCViewStatusBar>(memGame.HPBAR)) {
    bar->PSIZEX = std::max(1,int(180.f*scale));
    bar->PSIZEY = std::max(1,int(20.f*scale));
    bar->VSIZEX = int(180.f*scale*VirtualSize/float(uiWidth));
    bar->VSIZEY = int(20.f*scale*VirtualSize/float(uiHeight));
    }
  if(auto* viewport = mem32.deref<zCView>(memGame._ZCSESSION_VIEWPORT)) {
    viewport->PSIZEX = uiWidth;
    viewport->PSIZEY = uiHeight;
    }
  if(resized && resolutionFired) {
    HookRegs regs;
    fireHook(HookAdr::BarUpdateResolution,regs);
    auto* open = vm.find_symbol_by_name("CRAFTINGVIEW_ISOPEN");
    auto* show = vm.find_symbol_by_name("CRAFTINGVIEW_SHOW");
    auto* recipe = vm.find_symbol_by_name("CV_CURRENTITEM");
    auto* mode = vm.find_symbol_by_name("CV_CURRENTMODE");
    auto* amount = vm.find_symbol_by_name("CV_CURRENTAMOUNT");
    if(open!=nullptr && show!=nullptr && recipe!=nullptr && mode!=nullptr && amount!=nullptr && open->get_int()!=0) {
      withRecipeTemporary(uint32_t(recipe->get_int()),"CRAFTINGVIEW_SHOW",[&]() {
        vm.call_function(show,recipe->get_int(),mode->get_int(),amount->get_int());
        });
      }
    }
  spriteOverlay.clear();
  renderedViews.clear();
  uiBlend = 2;
  collectingSprites = true;
  try {
    HookRegs regs;
    fireHook(HookAdr::GameRender,regs);
    fireHook(HookAdr::SpriteRender,regs);
    }
  catch(...) {
    collectingSprites = false;
    throw;
    }
  collectingSprites = false;
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
  auto views = openViews;
  for(auto ptr:renderedViews)
    if(std::find(views.begin(),views.end(),ptr)==views.end())
      views.push_back(ptr);
  for(auto vp:views) {
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
  std::stable_sort(spriteOverlay.begin(),spriteOverlay.end(),[](const auto& a, const auto& b) {
    return a.priority<b.priority;
    });
  for(auto& sprite:spriteOverlay)
    out.push_back(std::move(sprite));
  spriteOverlay.clear();
  }
