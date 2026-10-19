#include "directmemory.h"

#include <Tempest/Log>

#include <algorithm>
#include <cstring>

#include "hookaddresses.h"
#include "logbudget.h"
#include "world/objects/item.h"
#include "game/inventory.h"
#include "world/objects/npc.h"

using namespace Tempest;
using namespace Compatibility;

/// A character took an item: oCNpcInventory::Insert. The hook reads ecx (the inventory, its owner) and esi (the item).
void DirectMemory::eventItemTaken(Npc& owner, Item& item) {
  if(!hasHook(HookAdr::InventoryInsert))
    return;
  auto* sym = vm.find_symbol_by_name("OCNPCINVENTORY.OWNER");
  if(sym==nullptr)
    return;

  const ptr32_t inventory = mem32.alloc(512,"oCNpcInventory");
  if(inventory==0)
    return;
  mem32.writeInt(inventory+sym->offset_as_member(),int32_t(ptrOf(owner)));

  HookRegs r;
  r.ecx = inventory;
  r.esi = ptrOf(item);
  fireHook(HookAdr::InventoryInsert,r);
  mem32.free(inventory);
  }

/// oCNpc::ChangeAttribute(attribute, change): the hook may change the change
int DirectMemory::eventChangeAttribute(Npc& npc, int attribute, int change) {
  if(!hasHook(HookAdr::ChangeAttribute))
    return change;
  HookRegs r;
  r.ecx  = ptrOf(npc);
  r.args = {attribute,change};
  fireHook(HookAdr::ChangeAttribute,r);
  return r.args[1];
  }

/// A key that nobody in the game has taken: oCGame::HandleEvent, the default case
void DirectMemory::eventKey(int key) {
  if(!hasHook(HookAdr::KeyEvent))
    return;
  HookRegs r;
  r.esi = uint32_t(key);
  fireHook(HookAdr::KeyEvent,r);
  }

/// The player has come into another zone of the music
void DirectMemory::eventZoneMusic(std::string_view name) {
  if(!hasHook(HookAdr::ZoneMusicChanged))
    return;
  const ptr32_t str = mem32.alloc(sizeof(zString),"zString");
  if(str==0)
    return;
  if(auto* s = mem32.deref<zString>(str)) {
    memAssignString(*s,name);
    s->_VTBL = 0;
    }
  HookRegs r;
  r.edx = str;
  fireHook(HookAdr::ZoneMusicChanged,r);
  if(auto* s = mem32.deref<zString>(str))
    mem32.free(s->ptr);
  mem32.free(str);
  }

/// oCNpc::CreateFallDamage: the hook sets the protection against the fall
void DirectMemory::eventFallDamage(Npc& npc) {
  if(!hasHook(HookAdr::CreateFallDamage))
    return;
  HookRegs r;
  r.ecx = ptrOf(npc);
  fireHook(HookAdr::CreateFallDamage,r);
  }

// --- damage: oCNpc::OnDamage with an oSDamageDescriptor, the scripts of CoM take the armor into account there

namespace {
float bitsToFloat(int32_t i) {
  float f = 0;
  std::memcpy(&f,&i,sizeof(f));
  return f;
  }
}

void DirectMemory::eventDamageBegin(Npc& victim, Npc& attacker, bool isSpell, int spellId, bool ranged, GameScript::DmaDamage& d) {
  if(!hasHook(HookAdr::OnDamage) && !hasHook(HookAdr::OnDamageScript))
    return;
  auto* cls = vm.find_symbol_by_name("OSDAMAGEDESCRIPTOR");
  if(cls==nullptr)
    return;
  auto field = [this](const char* name) -> int32_t {
    auto* s = vm.find_symbol_by_name(std::string("OSDAMAGEDESCRIPTOR.")+name);
    return s==nullptr ? -1 : int32_t(s->offset_as_member());
    };
  auto put = [&](const char* name, int32_t v) {
    const int32_t off = field(name);
    if(off>=0)
      mem32.writeInt(d.desc+ptr32_t(off),v);
    };

  d.desc = mem32.alloc(std::max<uint32_t>(cls->class_size(),160)+16,"oSDamageDescriptor");
  if(d.desc==0)
    return;

  const Item* weapon = attacker.inventory().activeWeapon();
  put("DWFIELDSVALID",0xFFFF);
  put("PVOBATTACKER", int32_t(ptrOf(attacker)));
  put("PNPCATTACKER", int32_t(ptrOf(attacker)));
  if(weapon!=nullptr && !isSpell)
    put("PITEMWEAPON",int32_t(ptrOf(const_cast<Item&>(*weapon))));
  put("NSPELLID",isSpell ? spellId : 0);
  // oETypeWeapon: 1 fist, 2 melee, 4 range, 8 magic
  put("ENUMODEWEAPON",isSpell ? 8 : (ranged ? 4 : (weapon!=nullptr ? 2 : 1)));
  put("BDAMAGEDONTKILL",d.dontKill ? 1 : 0);

  HookRegs r;
  r.ecx  = ptrOf(victim);
  r.args = {int32_t(d.desc)};
  fireHook(HookAdr::OnDamage,r);

  const int32_t off = field("BDAMAGEDONTKILL");
  if(off>=0)
    d.dontKill = mem32.readInt(d.desc+ptr32_t(off))!=0;
  }

void DirectMemory::eventDamageEnd(Npc& victim, GameScript::DmaDamage& d) {
  if(d.desc==0)
    return;

  HookRegs r;
  r.ecx  = ptrOf(victim);
  r.args = {int32_t(d.desc)};
  fireHook(HookAdr::OnDamageScript,r);

  // what the scripts have added to the total damage
  if(auto* s = vm.find_symbol_by_name("OSDAMAGEDESCRIPTOR.FDAMAGETOTAL")) {
    const float f = bitsToFloat(mem32.readInt(d.desc+s->offset_as_member()));
    if(f>=1.f)
      d.extra = int(f);
    }
  mem32.free(d.desc);
  d.desc = 0;
  }
