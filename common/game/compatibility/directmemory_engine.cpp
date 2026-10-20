#include "directmemory.h"

#include <Tempest/Log>

#include <cmath>
#include <cstring>

#include "world/aiqueue.h"
#include "logbudget.h"
#include "world/objects/item.h"
#include "world/objects/npc.h"
#include "world/world.h"

using namespace Tempest;
using namespace Compatibility;

// Functions of Gothic2.exe that the scripts of CoM call by the address (CALL__thiscall)
static constexpr uint32_t OCGAME__TRIGGERCHANGELEVEL = 7109360;
static constexpr uint32_t ZCVOB__SETPOSITIONWORLD    = 6404976;
static constexpr uint32_t OCNPC__GETMODEL            = 7571232;
static constexpr uint32_t ZCMODEL__STARTANIMATION    = 5746544;
static constexpr uint32_t ZCMODEL__ISANIMATIONACTIVE  = 5727888;
static constexpr uint32_t OCINFOMANAGER__GETINFORMATION = 7350544;

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
    if(std::abs(to.x-pos.x)>0.01f || std::abs(to.y-pos.y)>0.01f || std::abs(to.z-pos.z)>0.01f) {
      if(npc!=nullptr)
        npc->setPosition(to);
      else
        item->setPosition(to.x,to.y,to.z);
      }
    return;
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
  // the floats of Ikarus (float.d) are ints, the VM does not take a float for an int
  auto safeOverride = [this](std::string_view name, auto fn) {
    auto* f = vm.find_symbol_by_name(name);
    if(f!=nullptr && f->type()==zenkit::DaedalusDataType::FUNCTION)
      vm.override_function(name,fn);
    };
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

  // zCInput: there is an object, the keys are not bound by the scripts here
  const ptr32_t input = mem32.alloc(256,"zCInput");
  mem32.alloc(ZCINPUT_ZINPUT,4,"zinput*");
  mem32.writeInt(ZCINPUT_ZINPUT,int32_t(input));
  cpu.register_thiscall(ZCINPUT_WIN32__CLEARKEYBUFFER,[](ptr32_t) {
    });
  cpu.register_thiscall(ZCINPUT_WIN32__GETSTATE,[](ptr32_t, int) -> int32_t {
    return 0;
    });
  cpu.register_thiscall(ZCINPUT__ISBINDEDTOGGLED,[](ptr32_t, int, int) -> int32_t {
    return 0;
    });
  cpu.register_thiscall(ZCINPUT__BINDKEYS,[](ptr32_t, int) {
    });

  // oCGame::TriggerChangeLevel(level, point): the screen has faded and the mod goes to another world
  cpu.register_thiscall(OCGAME__TRIGGERCHANGELEVEL,[this](ptr32_t, std::string level, std::string point) {
    Log::i("DMA: change of the level ",level," at ",point);
    gameScript.world().triggerChangeWorld(level,point);
    });

  // zCVob::SetPositionWorld(vec3*)
  cpu.register_thiscall(ZCVOB__SETPOSITIONWORLD,[this](ptr32_t self, ptr32_t pvec) {
    auto* npc = npcOf(handles.instanceAt(self));
    auto* vec = mem32.deref<const float>(pvec);
    if(npc==nullptr || vec==nullptr) {
      if(logBudget("setposition"))
        Log::e("DMA: zCVob::SetPositionWorld of an object that is not a character");
      return;
      }
    float v[3] = {};
    std::memcpy(v,vec,sizeof(v));
    npc->setPosition(v[0],v[1],v[2]);
    });

  // the model of a character is the character here, an animation is started as AI_PlayAni does it
  cpu.register_thiscall(OCNPC__GETMODEL,[](ptr32_t self) -> int32_t {
    return int32_t(self);
    });

  // 0x688A70(this = a character, int on): CoM turns off with FALSE what its scripts call the detection of the chasm
  // (Q304 every frame for the bodyguards of the trap, Q204 for the hero). The original engine stops a character that walks
  // to the edge of a cliff; OpenGothic has no such detection, there is nothing to turn off.
  cpu.register_thiscall(6851184,[](ptr32_t, int32_t) {});

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
