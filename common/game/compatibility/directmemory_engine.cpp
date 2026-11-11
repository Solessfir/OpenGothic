#include "directmemory.h"

#include <Tempest/Log>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstring>

#include "world/aiqueue.h"
#include "logbudget.h"
#include "world/objects/item.h"
#include "world/objects/npc.h"
#include "world/world.h"
#include "world/worldsound.h"
#include "gothic.h"
#include "gamemusic.h"
#include "graphics/worldview.h"
#include "resources.h"

using namespace Tempest;
using namespace Compatibility;

// Functions of Gothic2.exe that the scripts of CoM call by the address (CALL__thiscall)
static constexpr uint32_t OCGAME__TRIGGERCHANGELEVEL = 7109360;
static constexpr uint32_t ZCVOB__SETPOSITIONWORLD    = 6404976;
static constexpr uint32_t OCNPC__GETMODEL            = 7571232;
static constexpr uint32_t ZCMODEL__STARTANIMATION    = 5746544;
static constexpr uint32_t ZCMODEL__ISANIMATIONACTIVE  = 5727888;
static constexpr uint32_t OCINFOMANAGER__GETINFORMATION = 7350544;
static constexpr uint32_t ZCSNDSYS_MSS__REMOVEALLACTIVESOUNDS = 5167008;

/// The matrix of the vob (zCVob.trafoObjToWorld) is at 60 in the memory of a character and of an item
static constexpr size_t   TRAFO = 60;

namespace {
Npc* npcOf(const std::shared_ptr<zenkit::DaedalusInstance>& inst) {
  if(inst==nullptr || dynamic_cast<zenkit::INpc*>(inst.get())==nullptr)
    return nullptr;
  return reinterpret_cast<Npc*>(inst->user_ptr);
  }
}

/// The memory of a character or an item: the place in the world (a script writes it to teleport)
void DirectMemory::syncHandleObject(uint8_t* raw, zenkit::DaedalusInstance& inst, bool write) {
  Npc*  npc  = nullptr;
  Item* item = nullptr;
  if(dynamic_cast<zenkit::INpc*>(&inst)!=nullptr)
    npc = reinterpret_cast<Npc*>(inst.user_ptr);
  else if(dynamic_cast<zenkit::IItem*>(&inst)!=nullptr)
    item = reinterpret_cast<Item*>(inst.user_ptr);
  if(npc==nullptr && item==nullptr)
    return;

  const Tempest::Vec3 pos = npc!=nullptr ? npc->position() : item->position();
  float m[16] = {};
  std::memcpy(m,raw+TRAFO,sizeof(m));

  if(write) {
    // a script has moved it
    const Tempest::Vec3 to(m[3],m[7],m[11]);
    if(!std::isfinite(to.x) || !std::isfinite(to.y) || !std::isfinite(to.z))
      return;
    if(std::abs(to.x-pos.x)>0.01f || std::abs(to.y-pos.y)>0.01f || std::abs(to.z-pos.z)>0.01f) {
      if(npc!=nullptr)
        npc->setPosition(to);
      else
        item->setPosition(to.x,to.y,to.z);
      }
    return;
    }

  if(item!=nullptr) {
    // the texts and the numbers of an item (the lines of a recipe), the scripts read them through _@S(item.text[0]) and
    // MEM_ReadStringArray: Ikarus has the strings of the engine, 20 bytes each
    auto* isym = dynamic_cast<zenkit::IItem*>(&inst);
    auto* tsym = vm.find_symbol_by_name("OCITEM.TEXT");
    auto* csym = vm.find_symbol_by_name("OCITEM.COUNT");
    if(isym!=nullptr && tsym!=nullptr && csym!=nullptr) {
      const size_t tOff = tsym->offset_as_member();
      const size_t cOff = csym->offset_as_member();
      for(uint32_t i=0; i<zenkit::IItem::text_count; ++i) {
        const size_t at = tOff + i*sizeof(zString);
        if(at+sizeof(zString)<=InstanceHandles::SlotSize) {
          auto* zs = reinterpret_cast<zString*>(raw+at);
          if(zs->ptr==0 || zs->len!=int32_t(isym->text[i].size()))
            memAssignString(*zs,isym->text[i]);
          }
        if(cOff+(i+1)*sizeof(int32_t)<=InstanceHandles::SlotSize) {
          const int32_t cnt = isym->count[i];
          std::memcpy(raw+cOff+i*sizeof(int32_t),&cnt,sizeof(cnt));
          }
        }
      }
    }

  const float a = npc!=nullptr ? npc->rotationRad() : 0.f;
  const float c = std::cos(a), s = std::sin(a);
  m[0] = c;  m[1] = 0; m[2]  = s;  m[3]  = pos.x;
  m[4] = 0;  m[5] = 1; m[6]  = 0;  m[7]  = pos.y;
  m[8] = -s; m[9] = 0; m[10] = c;  m[11] = pos.z;
  m[12] = 0; m[13] = 0; m[14] = 0; m[15] = 1;
  std::memcpy(raw+TRAFO,m,sizeof(m));
  }

// the input of Gothic (zCInput) that the scripts of CoM use for the keys
static constexpr uint32_t ZCINPUT_ZINPUT            = 9246288;   // the pointer to the object
static constexpr uint32_t ZCINPUT_WIN32__CLEARKEYBUFFER = 5068240;
static constexpr uint32_t ZCINPUT_WIN32__GETSTATE   = 5066032;
static constexpr uint32_t ZCINPUT__ISBINDEDTOGGLED  = 5031024;
static constexpr uint32_t ZCINPUT__BINDKEYS         = 5045760;

void DirectMemory::setupEngineCalls() {
  // zCAICamera::StartDialogCam(int), as requested by DIACAM_UPDATE in CoM.
  // OpenGothic owns the camera object; do not dereference Gothic's AI pointer.
  cpu.register_thiscall(4923632,[this](ptr32_t, int) {
    if(!dialogCameraOff && Gothic::inst().isInDialog())
      Gothic::inst().onDialogCameraReset();
    });
  // STOPALLSOUNDS / STOPALLACTIVESOUNDS in CoM's cutscenes. Music has a
  // separate engine interface and keeps playing, as in Gothic's sound system.
  cpu.register_thiscall(ZCSNDSYS_MSS__REMOVEALLACTIVESOUNDS,[this](ptr32_t) {
    gameScript.world().sound()->stopAllSounds();
    Gothic::inst().stopGlobalSounds();
    Gothic::inst().onStopAllSounds();
    });
  // the floats of Ikarus (float.d) are ints, the VM does not take a float for an int
  auto safeOverride = [this](std::string_view name, auto fn) {
    auto* f = vm.find_symbol_by_name(name);
    if(f!=nullptr && f->type()==zenkit::DaedalusDataType::FUNCTION)
      vm.override_function(name,fn);
    };
  safeOverride("ENABLEMUSICZONE",[this](std::string_view name, int enabled) {
    if(gameScript.world().sound()->setZoneEnabled(name,enabled!=0)==0 && logBudget("music-zone-enable"))
      Log::e("EnableMusicZone: no such zone: ",name);
    });
  if(vm.find_symbol_by_name("CATINV_ACTIVECATEGORY")!=nullptr &&
     vm.find_symbol_by_name("INV_CAT_GROUPS")!=nullptr) {
    safeOverride("CATINV_SETCATEGORY",[this](int index) -> int {
      const auto before = gameScript.inventoryScriptCategory();
      gameScript.setInventoryScriptCategory(index);
      // InventoryMenu reads this state and rebuilds its filtered iterators.
      // The original CATINV_UPDATEALL rebuilds Gothic's linked lists, which
      // are not the inventory backing OpenGothic's menu.
      return before!=gameScript.inventoryScriptCategory() ? 1 : 0;
      });
    }
  // CoM reloads one zCTexture in place through Ikarus to repaint the player's
  // house. Adapt that operation to renderer materials, using the script's own
  // filenames and saved choice. No fake zCTexture pointer is needed here.
  if(vm.find_symbol_by_name("HOUSE_CRAFT_WALL_CHANGETEXTURE")!=nullptr &&
     vm.find_symbol_by_name("APPLYHOUSEWALLTEXTURE.DEFAULTWALLTEXTURE")!=nullptr &&
     vm.find_symbol_by_name("APPLYHOUSEWALLTEXTURE.WALLTEXTUREPREFIX")!=nullptr) {
    safeOverride("APPLYHOUSEWALLTEXTURE",[this]() {
      const auto choice = vm.find_symbol_by_name("HOUSE_CRAFT_WALL_CHANGETEXTURE")->get_int();
      const auto original = vm.find_symbol_by_name("APPLYHOUSEWALLTEXTURE.DEFAULTWALLTEXTURE")->get_string();
      const auto prefix = vm.find_symbol_by_name("APPLYHOUSEWALLTEXTURE.WALLTEXTUREPREFIX")->get_string();
      std::string selected(original);
      if(choice>0)
        selected = std::string(prefix) + (choice<10 ? "0" : "") + std::to_string(choice) + ".TGA";
      auto* view = gameScript.world().view();
      if(view==nullptr)
        return;
      const auto* source = Resources::loadTexture(original);
      const auto* target = Resources::loadTexture(selected);
      if(source==nullptr || target==nullptr) {
        Log::e("CoM house walls: missing texture ",source==nullptr ? std::string(original) : selected);
        return;
        }
      if(view->replaceTexture(*source,*target))
        Log::i("CoM house walls: ",original," -> ",selected);
      if(auto* name = vm.find_symbol_by_name("APPLYHOUSEWALLTEXTURE.TEXNAME"))
        name->set_string(selected);
      });
    }
  safeOverride("MUSIC_OVERRIDETRACK",[this](int track) {
    if(auto* value = vm.find_symbol_by_name("MUSIC_CURRENTOVERRIDE"))
      value->set_int(track);
    GameMusic::inst().overrideTrack(uint32_t(track));
    });
  safeOverride("MUSIC_DISABLEOVERRIDE",[this]() {
    if(auto* value = vm.find_symbol_by_name("MUSIC_CURRENTOVERRIDE"))
      value->set_int(0);
    GameMusic::inst().clearTrackOverride();
    });
  safeOverride("CASTFROMINTF",[](int v) -> float {
    float f = 0;
    std::memcpy(&f,&v,sizeof(f));
    return f;
    });
  safeOverride("CASTTOINTF",[](float f) -> int {
    int v = 0;
    std::memcpy(&v,&f,sizeof(v));
    return v;
    });

  // the state of the keys: MEM_KeyPressed(key) reads a byte of this table
  mem32.pin(keyEvents,9246328,uint32_t(sizeof(keyEvents)),"key events");

  // UPDATE_MENU_ITEM(item, text) of CoM changes the text of an item of the menu that is open (the status screen): the
  // menus of OpenGothic are not in the memory of the layer, the texts wait until the menu takes them
  safeOverride("UPDATE_MENU_ITEM",[this](std::string_view item, std::string_view text) {
    std::string name(item);
    for(char& c:name)
      c = char(std::toupper(static_cast<unsigned char>(c)));
    menuTexts.emplace_back(std::move(name),std::string(text));
    });

  // zSinCosApprox(angle, &sin, &cos): the sprites of LeGo rotate with it
  cpu.register_cdecl(6269632,[this](float angle, ptr32_t sinPtr, ptr32_t cosPtr) {
    auto bits = [](float f) {
      int32_t i = 0;
      std::memcpy(&i,&f,sizeof(i));
      return i;
      };
    if(sinPtr!=0)
      mem32.writeInt(sinPtr,bits(std::sin(angle)));
    if(cosPtr!=0)
      mem32.writeInt(cosPtr,bits(std::cos(angle)));
    });

  // what the scripts do to the objects of the engine that the layer does not have: they are made and removed without
  // any effect (MEM_DeleteVob, MEM_RenameVob, MEM_ApplyGothOpt)
  cpu.register_thiscall(7864512,[](ptr32_t, ptr32_t) {});            // oCWorld::RemoveVob
  cpu.register_thiscall(6290896,[](ptr32_t, std::string) {});         // zCVob::SetVobName
  cpu.register_thiscall(4355760,[](ptr32_t) {});                      // CGameManager::ApplySomeSettings

  // oCNpc::CloseInventory: the scripts close the inventory of the player
  cpu.register_thiscall(7742480,[this](ptr32_t) {
    inventoryClose = true;
    });

  // zCEventMessage::SetDeleted(1): the hook of EV_DropVob finishes the message at once, the drop is cancelled
  cpu.register_thiscall(7745808,[this](ptr32_t self, int32_t flag) {
    if(self!=0 && self==dropMessage && flag!=0)
      dropCancelled = true;
    });

  // zCInput: script bindings share the native UI's current KEYS options.
  const ptr32_t input = mem32.alloc(256,"zCInput");
  mem32.alloc(ZCINPUT_ZINPUT,4,"zinput*");
  mem32.writeInt(ZCINPUT_ZINPUT,int32_t(input));
  auto bindings = [](std::string_view option) {
    std::array<int32_t,2> result = {};
    const auto value = Gothic::settingsGetS("KEYS",option);
    for(size_t i=0; i<result.size(); ++i) {
      if(value.size()<(i+1)*4)
        break;
      uint32_t packed = 0;
      const auto* begin = value.data()+i*4;
      const auto parsed = std::from_chars(begin,begin+4,packed,16);
      if(parsed.ec!=std::errc() || parsed.ptr!=begin+4)
        continue;
      int32_t code = int32_t(((packed&255)<<8) | ((packed>>8)&255));
      // OpenGothic's key editor stores its digit shortcuts as 120..129.
      // Ikarus must see the corresponding DirectInput digit scans 2..11.
      if(code>=120 && code<=129)
        code = code-120+2;
      result[i] = code;
      }
    return result;
    };
  safeOverride("MEM_GetKey",[bindings](std::string_view option) -> int32_t {
    return bindings(option)[0];
    });
  safeOverride("MEM_GetSecondaryKey",[bindings](std::string_view option) -> int32_t {
    return bindings(option)[1];
    });
  safeOverride("MEM_SetKeys",[this](std::string_view option, int primary, int secondary) {
    if(option.empty())
      return;
    // Ikarus stores two uint16 DirectInput codes, little endian, as hex.
    // INIT_TORCHKEY relies on this to install its defaults in a fresh INI.
    constexpr char hex[] = "0123456789ABCDEF";
    std::string value(8,'0');
    const std::array<uint16_t,2> codes = {uint16_t(primary),uint16_t(secondary)};
    for(size_t i=0; i<codes.size(); ++i) {
      const auto code = codes[i];
      value[i*4+0] = hex[(code>>4)&15];
      value[i*4+1] = hex[code&15];
      value[i*4+2] = hex[(code>>12)&15];
      value[i*4+3] = hex[(code>>8)&15];
      }
    Gothic::settingsSetS("KEYS",option,value);
    std::fill(std::begin(keyToggles),std::end(keyToggles),uint8_t(0));
    });
  auto weaponAction = [this](int action) {
    const auto* symbol = vm.find_symbol_by_name("ZOPT_GAMEKEY_WEAPON");
    return action==(symbol!=nullptr ? symbol->get_int() : 8);
    };
  cpu.register_thiscall(ZCINPUT_WIN32__CLEARKEYBUFFER,[this](ptr32_t) {
    std::fill(std::begin(keyEvents),std::end(keyEvents),uint8_t(0));
    std::fill(std::begin(keyToggles),std::end(keyToggles),uint8_t(0));
    });
  cpu.register_thiscall(ZCINPUT_WIN32__GETSTATE,[this,bindings,weaponAction](ptr32_t, int action) -> float {
    // The CoM DAT calls this only for ZOPT_GAMEKEY_WEAPON (8).
    if(!weaponAction(action)) {
      if(logBudget("input-state-action"))
        Log::e("Ikarus input: unsupported logical action ",action);
      return 0.f;
      }
    for(const auto code:bindings("keyWeapon"))
      if(code>0 && size_t(code)<sizeof(keyEvents) && keyEvents[code]!=0)
        return 1.f;
    return 0.f;
    });
  cpu.register_thiscall(ZCINPUT__ISBINDEDTOGGLED,[this,bindings,weaponAction](ptr32_t, int action, int key) -> int32_t {
    if(!weaponAction(action) || key<=0 || size_t(key)>=sizeof(keyToggles))
      return 0;
    for(const auto code:bindings("keyWeapon")) {
      if(code!=key || keyToggles[key]==0)
        continue;
      keyToggles[key] = 0;
      return 1;
      }
    return 0;
    });
  cpu.register_thiscall(ZCINPUT__BINDKEYS,[this](ptr32_t, int) {
    // Bindings are read from the same current KEYS options as the native UI;
    // rebinding discards old toggles without maintaining a second key map.
    std::fill(std::begin(keyToggles),std::end(keyToggles),uint8_t(0));
    });
  cpu.register_float_to_int(0x7D0F48); // __ftol, used after GetState in CoM

  // oCGame::TriggerChangeLevel(level, point): the screen has faded and the mod goes to another world
  cpu.register_thiscall(OCGAME__TRIGGERCHANGELEVEL,[this](ptr32_t, std::string level, std::string point) {
    Log::i("DMA: change of the level ",level," at ",point);
    gameScript.world().triggerChangeWorld(level,point);
    });

  // oCNpc::DoTakeVob(zCVob*), used by CoM's underwater pickup hook. The
  // callback queues T_WATERTAKEITEM separately, so move the real loose item
  // without starting a second ground/pickup animation. Other vob types are
  // outside this callback's item-only contract.
  cpu.register_thiscall(7621056,[this](ptr32_t self, ptr32_t object) -> int32_t {
    auto* npc = npcOf(handles.instanceAt(self));
    auto ref = handles.instanceAt(object);
    auto* item = ref!=nullptr && dynamic_cast<zenkit::IItem*>(ref.get())!=nullptr
                 ? reinterpret_cast<Item*>(ref->user_ptr) : nullptr;
    return npc!=nullptr && item!=nullptr && npc->takeItem(*item,false)!=nullptr ? 1 : 0;
    });

  // oCMag_Book::SetOwner(zCVob*), the owner/world setup that CoM's Slowdown
  // checks before its ordinary spell script. Offsets 0x18 and 0x1c are also
  // confirmed by the original engine's instructions at 0x475a50.
  cpu.register_thiscall(4676176,[this](ptr32_t self, ptr32_t owner) {
    auto it = magicBooks.find(self);
    auto ref = it!=magicBooks.end() ? it->second.lock() : nullptr;
    if(ref==nullptr || ref->user_ptr==nullptr || handles.instanceAt(owner)!=ref)
      return;
    mem32.writeInt(self+0x18,int32_t(memGame._ZCSESSION_WORLD));
    mem32.writeInt(self+0x1c,int32_t(owner));
    mem32.writeInt(self+0x20,int32_t(owner)); // GetModel uses the NPC handle here.
    });

  // zCVob::SetPositionWorld(vec3*)
  cpu.register_thiscall(ZCVOB__SETPOSITIONWORLD,[this](ptr32_t self, ptr32_t pvec) {
    auto* npc = npcOf(handles.instanceAt(self));
    float v[3] = {};
    const auto* vec = mem32.deref(pvec,uint32_t(sizeof(v)));
    if(npc==nullptr || vec==nullptr) {
      if(logBudget("setposition"))
        Log::e("DMA: zCVob::SetPositionWorld of an object that is not a character");
      return;
      }
    std::memcpy(v,vec,sizeof(v));
    if(!std::isfinite(v[0]) || !std::isfinite(v[1]) || !std::isfinite(v[2]))
      return;
    npc->setPosition(v[0],v[1],v[2]);
    });

  // the model of a character is the character here, an animation is started as AI_PlayAni does it
  cpu.register_thiscall(OCNPC__GETMODEL,[](ptr32_t self) -> int32_t {
    return int32_t(self);
    });

  // oCNpc::SetWalkStopChasm(int). The installed Gothic2.exe confirms this
  // sets bit 2 at GetHumanAI()+0xb8; it does not disable world collision.
  // CoM uses FALSE in Q204 and every frame for Q304's trap bodyguards.
  const ptr32_t OCNPC__SETWALKSTOPCHASM = 6851184; // 0x688a70
  cpu.register_thiscall(OCNPC__SETWALKSTOPCHASM,[this](ptr32_t self, int32_t enabled) {
    if(auto* npc = npcOf(handles.instanceAt(self)))
      npc->setWalkStopChasm(enabled!=0);
    });

  // oCInfoManager::GetInformation(id): an oCInfo of the memory. SetDialogueToldProperty of CoM writes OCINFO.TOLD, the game takes
  // it over in tickInfos
  cpu.register_thiscall(OCINFOMANAGER__GETINFORMATION,[this](ptr32_t, int32_t id) -> int32_t {
    const ptr32_t obj = mem32.alloc(128,"oCInfo");
    if(obj==0)
      return 0;
    auto* told = vm.find_symbol_by_name("OCINFO.TOLD");
    const int32_t off = told==nullptr ? -1 : int32_t(told->offset_as_member());
    const int32_t now = gameScript.isInfoTold(size_t(id)) ? 1 : 0;
    if(off>=0)
      mem32.writeInt(obj+ptr32_t(off),now);
    pendingInfos.push_back({obj,size_t(id),now});
    return int32_t(obj);
    });

  // zCModel::IsAnimationActive(name): ANIISACTIVE of CoM, the animation is among the layers that the character plays
  cpu.register_thiscall(ZCMODEL__ISANIMATIONACTIVE,[this](ptr32_t self, std::string name) -> int32_t {
    auto* npc = npcOf(handles.instanceAt(self));
    if(npc==nullptr)
      return 0;
    for(char& c:name)
      c = char(std::toupper(static_cast<unsigned char>(c)));
    for(auto& l:npc->animationLayers())
      if(l.name==name)
        return 1;
    return 0;
    });

  cpu.register_thiscall(ZCMODEL__STARTANIMATION,[this](ptr32_t self, std::string name, int) -> int32_t {
    auto* npc = npcOf(handles.instanceAt(self));
    if(npc==nullptr)
      return 0;
    npc->aiPush(AiQueue::aiPlayAnim(name));
    return 1;
    });
  }
