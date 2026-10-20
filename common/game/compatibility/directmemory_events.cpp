#include "directmemory.h"

#include <Tempest/Log>

#include <algorithm>
#include <cstring>

#include "hookaddresses.h"
#include "logbudget.h"
#include "world/focus.h"
#include "world/objects/interactive.h"
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

// --- the focus: the pointer to the vob that the player looks at, chests and doors as oCMobLockable

/// The pointer of a chest or a door for the scripts: oCMobLockable with the lock of the mob
auto DirectMemory::mobPointer(Interactive& mob) -> ptr32_t {
  if(mobBase==0)
    return 0;
  auto it = mobIndex.find(&mob);
  if(it!=mobIndex.end())
    return mobBase + it->second*uint32_t(sizeof(Compatibility::MobSlot));

  // the slots are used in turns
  const uint32_t i = mobCursor;
  mobCursor = (mobCursor+1) % MobSlots;
  if(mobs[i]!=nullptr)
    mobIndex.erase(mobs[i]);
  mobs[i]    = &mob;
  mobPick[i] = 0;
  mobIndex[&mob] = i;
  return mobBase + i*uint32_t(sizeof(Compatibility::MobSlot));
  }

void DirectMemory::clearMobs() {
  std::fill(mobs.begin(),mobs.end(),nullptr);
  mobIndex.clear();
  lastSpellFocus = -1;
  }

void DirectMemory::memoryCallback(Compatibility::MobSlot& m, uint32_t index, std::memory_order ord) {
  if(index>=mobs.size() || mobs[index]==nullptr)
    return;
  auto* mob = mobs[index];

  auto offset = [this](const char* name) -> int32_t {
    auto* s = vm.find_symbol_by_name(std::string("OCMOBLOCKABLE.")+name);
    return s==nullptr ? -1 : int32_t(s->offset_as_member());
    };
  auto put = [&](int32_t off, int32_t v) {
    if(off>=0 && size_t(off)+4<=sizeof(m.raw))
      std::memcpy(m.raw+off,&v,4);
    };
  auto get = [&](int32_t off) -> int32_t {
    int32_t v = 0;
    if(off>=0 && size_t(off)+4<=sizeof(m.raw))
      std::memcpy(&v,m.raw+off,4);
    return v;
    };

  // oCMobLockable: bit 0 is the lock, bit 1 opens by itself, the rest is the number of the character of the lock that is done
  const bool locked = !mob->pickLockCode().empty() && !mob->isCracked();

  if(ord==std::memory_order::release) {
    const int32_t bf = get(offset("BITFIELD"));
    const bool    nowLocked = (bf&1)!=0;
    mobPick[index] = uint32_t(bf)>>2;
    if(nowLocked!=locked && !mob->pickLockCode().empty()) {
      mob->setAsCracked(!nowLocked);
      if(!nowLocked)
        lockOpenedByScript = true;
      }
    return;
    }

  // the table of virtual functions says what it is: a chest, a door or another mob
  const int32_t vtbl = mob->isContainer() ? 8637284 : (mob->isDoor() ? 8638548 : 8639884);
  put(0,vtbl);
  put(offset("BITFIELD"),(locked ? 1 : 0) | int32_t(mobPick[index]<<2));

  auto text = [&](const char* name, std::string_view v) {
    const int32_t off = offset(name);
    if(off<0 || size_t(off)+sizeof(zString)>sizeof(m.raw))
      return;
    zString z;
    std::memcpy(&z,m.raw+off,sizeof(z));
    memAssignString(z,v);
    std::memcpy(m.raw+off,&z,sizeof(z));
    };
  text("PICKLOCKSTR",mob->pickLockCode());
  text("KEYINSTANCE",mob->keyName());
  }

/// The player looks at a character, an item or a mob: oCNpc.focus_vob is the pointer
void DirectMemory::eventFocus(Npc& player, const Focus& focus) {
  int32_t ptr = 0;
  if(focus.npc!=nullptr)
    ptr = int32_t(ptrOf(*focus.npc));
  else if(focus.item!=nullptr)
    ptr = int32_t(ptrOf(*focus.item));
  else if(focus.interactive!=nullptr)
    ptr = int32_t(mobPointer(*focus.interactive));
  player.handle().compat_focus_vob = ptr;
  }

// --- the magic: Focus_Magic of the scripts is a place in the memory that the lock picking spell of CoM changes

void DirectMemory::setupFocusMagic() {
  auto* cls = vm.find_symbol_by_name("C_FOCUS");
  if(cls==nullptr || vm.find_symbol_by_name("FOCUS_MAGIC")==nullptr)
    return;

  focusMagic = mem32.alloc(std::max<uint32_t>(cls->class_size(),128)+16,"Focus_Magic");
  if(focusMagic==0)
    return;

  // the values that the game has now
  const zenkit::IFocus& f = gameScript.focusMage();
  auto putF = [&](const char* name, float v) {
    if(auto* m = vm.find_symbol_by_name(std::string("C_FOCUS.")+name)) {
      int32_t bits = 0;
      std::memcpy(&bits,&v,sizeof(bits));
      mem32.writeInt(focusMagic+m->offset_as_member(),bits);
      }
    };
  auto putI = [&](const char* name, int32_t v) {
    if(auto* m = vm.find_symbol_by_name(std::string("C_FOCUS.")+name))
      mem32.writeInt(focusMagic+m->offset_as_member(),v);
    };
  putF("NPC_LONGRANGE",f.npc_longrange);
  putF("NPC_RANGE1",f.npc_range1);   putF("NPC_RANGE2",f.npc_range2);
  putF("NPC_AZI",f.npc_azi);         putF("NPC_ELEVDO",f.npc_elevdo);   putF("NPC_ELEVUP",f.npc_elevup);
  putI("NPC_PRIO",f.npc_prio);
  putF("ITEM_RANGE1",f.item_range1); putF("ITEM_RANGE2",f.item_range2);
  putF("ITEM_AZI",f.item_azi);       putF("ITEM_ELEVDO",f.item_elevdo); putF("ITEM_ELEVUP",f.item_elevup);
  putI("ITEM_PRIO",f.item_prio);
  putF("MOB_RANGE1",f.mob_range1);   putF("MOB_RANGE2",f.mob_range2);
  putF("MOB_AZI",f.mob_azi);         putF("MOB_ELEVDO",f.mob_elevdo);   putF("MOB_ELEVUP",f.mob_elevup);
  putI("MOB_PRIO",f.mob_prio);

  // oCNpcFocus::focusList[5]: the script takes the instance from there (SPELL_PICKLOCK_INIT_ALWAYS)
  focusList[5] = focusMagic;
  }

/// What the scripts have written to Focus_Magic goes to the focus that the game uses
void DirectMemory::syncFocusMage(zenkit::IFocus& focus) {
  if(focusMagic==0)
    return;
  auto read = [this](const char* name, int32_t& value) {
    auto* s = vm.find_symbol_by_name(std::string("C_FOCUS.")+name);
    if(s!=nullptr)
      value = mem32.readInt(focusMagic+s->offset_as_member());
    };
  read("NPC_PRIO", focus.npc_prio);
  read("ITEM_PRIO",focus.item_prio);
  read("MOB_PRIO", focus.mob_prio);
  }

/// oCSpell::Setup: the player has taken a spell. The hook reads the caster ([esp+4]) and the spell id ([ecx+84])
void DirectMemory::eventSpellFocus(Npc& caster, int spellId) {
  if(!hasHook(HookAdr::SpellSetup) || lastSpellFocus==spellId)
    return;
  lastSpellFocus = spellId;

  const ptr32_t spell = mem32.alloc(256,"oCSpell");
  if(spell==0)
    return;
  mem32.writeInt(spell+84,spellId);
  HookRegs r;
  r.ecx  = spell;
  r.args = {int32_t(ptrOf(caster))};
  fireHook(HookAdr::SpellSetup,r);
  mem32.free(spell);
  }

/// The player tries to open a chest or a door that is locked: the mods can open it (the talent of picking locks of CoM)
void DirectMemory::eventMobCanOpen(Interactive& mob) {
  if(!hasHook(HookAdr::MobCanOpen))
    return;
  HookRegs r;
  r.ecx = mobPointer(mob);
  fireHook(HookAdr::MobCanOpen,r);
  }

/// A key is down or up: MEM_KeyPressed and MEM_KeyState of Ikarus read it
void DirectMemory::eventKeyState(int key, bool down) {
  if(key>0 && size_t(key)<sizeof(keyEvents))
    keyEvents[key] = down ? 1 : 0;
  }

/// The status screen of the player is shown or closed: the hooks of CoM write the texts of its items (UPDATE_MENU_ITEM)
void DirectMemory::eventStatusScreen(bool show) {
  const uint32_t adr = show ? HookAdr::StatusScreenShow : HookAdr::StatusScreenClose;
  if(!hasHook(adr))
    return;
  HookRegs r;
  fireHook(adr,r);
  }

auto DirectMemory::takeMenuTexts() -> std::vector<std::pair<std::string,std::string>> {
  auto ret = std::move(menuTexts);
  menuTexts.clear();
  return ret;
  }

/// A camera of a cutscene starts or ends (zCCSCamera::OnTrigger and OnUntrigger): CoM keeps the state of its cutscenes
void DirectMemory::eventCsCamera(bool trigger) {
  const uint32_t adr = trigger ? HookAdr::CsCameraTrigger : HookAdr::CsCameraUntrigger;
  if(!hasHook(adr))
    return;
  if(csCamera==0)
    csCamera = mem32.alloc(512,"zCCSCamera");
  HookRegs r;
  r.eax = csCamera;
  fireHook(adr,r);
  }

/// The player drops an item (oCNpc::EV_DropVob). The hook of CoM keeps the important items: it plays "no" and finishes
/// the message at once, true means that the drop is cancelled.
bool DirectMemory::eventDropVob(Npc& npc, Item& item) {
  if(!hasHook(HookAdr::EvDropVob))
    return false;

  // oCMsgManipulate: the item is at +108
  const ptr32_t msg = mem32.alloc(128,"oCMsgManipulate");
  if(msg==0)
    return false;
  mem32.writeInt(msg+108,int32_t(ptrOf(item)));

  dropMessage   = msg;
  dropCancelled = false;
  HookRegs r;
  r.ecx = ptrOf(npc);
  r.args.push_back(int32_t(msg));
  fireHook(HookAdr::EvDropVob,r);

  const bool cancelled = dropCancelled;
  dropMessage   = 0;
  dropCancelled = false;
  mem32.free(msg);
  return cancelled;
  }

/// The player opens the inventory (oCNpc::OpenInventory). CoM hooks it to close the inventory again during a fistfight
/// (the function that it queues calls oCNpc::CloseInventory, see takeInventoryClose).
void DirectMemory::eventInventoryOpen(Npc& npc) {
  if(!hasHook(HookAdr::InventoryOpen))
    return;
  HookRegs r;
  r.ecx = ptrOf(npc);
  fireHook(HookAdr::InventoryOpen,r);
  }

bool DirectMemory::takeInventoryClose() {
  const bool ret = inventoryClose;
  inventoryClose = false;
  return ret;
  }

/// The player sells an item (oCViewDialogTrade::OnTransferLeft). ITEMIMPORTANTFLAG_CHECKSELLINGITEMS of CoM, read from
/// its code in the DAT (tools/ikarus/disasm.py): an item with ITEM_IMPORTANT is not sold, the hero shows T_CANNOTTAKE.
/// True means that the sale is cancelled.
bool DirectMemory::eventSellItem(Npc& npc, Item& item) {
  if(!hasHook(HookAdr::SellItem))
    return false;
  constexpr int32_t ITEM_IMPORTANT = 134217728;
  if((int32_t(item.handle().flags) & ITEM_IMPORTANT)!=ITEM_IMPORTANT)
    return false;
  npc.playAnimByName("T_CANNOTTAKE",BS_NONE);
  return true;
  }

/// The player starts a finishing move (oCNpc::EV_FinishingMove). EV_FINISHINGMOVE_HOOK of CoM (read from its code in the
/// DAT): it keeps FINISHANIM, the flag of its own animations of the finishing move, and for an unconscious character
/// with NPC_FLAG_IMPORTANT it plays "no" and lengthens the sleep. OpenGothic does not start the move then.
bool DirectMemory::eventFinishingMove(Npc& npc, Npc& target) {
  if(!hasHook(HookAdr::FinishingMove))
    return false;
  // the hook reads the victim from oCNpc.focus_vob: with a weapon in the hand the focus of OpenGothic skips a character
  // that is down, the original engine has him in the focus
  npc.handle().compat_focus_vob = int32_t(ptrOf(target));
  HookRegs r;
  r.ecx = ptrOf(npc);
  finishHook    = true;
  finishRefused = false;
  fireHook(HookAdr::FinishingMove,r);
  finishHook = false;
  // the hook refuses an important character (T_NO): the move does not start, the hero only shakes his head
  const bool refused = finishRefused;
  finishRefused = false;
  return refused;
  }

void DirectMemory::noteAiPlayAni(Npc& npc, std::string_view name) {
  if(finishHook && npc.isPlayer() && name.size()==4 && (name=="T_NO" || name=="t_no"))
    finishRefused = true;
  }

/// The code of a lock that the player has to enter (oCMobLockable): HANDLE_SETPROPERPICKLOCKSTRLEN of CoM shortens it by
/// the talent and by the bonus (GETPICKLOCKSTRLEN). Returns the length that has to be entered.
size_t DirectMemory::eventPickLockLength(Interactive& mob, size_t length) {
  if(!hasHook(HookAdr::PickLockLength))
    return length;
  HookRegs r;
  r.esi = mobPointer(mob);
  r.ecx = uint32_t(length);
  fireHook(HookAdr::PickLockLength,r);
  const int32_t len = int32_t(r.ecx);
  if(len<0)
    return 0;
  return std::min(size_t(len),length);
  }

/// A spell starts to be cast: no lock is opened yet
void DirectMemory::eventSpellBegin() {
  lockOpenedByScript = false;
  }

/// The scroll of the lock picking spell of CoM (SPL_PICKLOCK) is spent only when the spell has opened the lock: the
/// player may let the key go early, the progress stays in the lock (the number of the character that is done).
bool DirectMemory::spellKeepsItem(int spellId) {
  auto* sym = vm.find_symbol_by_name("SPL_PICKLOCK");
  if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::INT || sym->get_int()!=spellId)
    return false;
  const bool opened = lockOpenedByScript;
  lockOpenedByScript = false;
  return !opened;
  }
