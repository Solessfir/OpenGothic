#include "directmemory.h"

#include <Tempest/Log>

#include <algorithm>
#include <cstring>
#include <cmath>
#include <limits>

#include "hookaddresses.h"
#include "graphics/effect.h"
#include "logbudget.h"
#include "world/focus.h"
#include "world/objects/interactive.h"
#include "world/objects/item.h"
#include "game/inventory.h"
#include "world/objects/npc.h"
#include "world/world.h"
#include "gothic.h"

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
  auto putFloat = [&](const char* name, float v) {
    int32_t bits = 0;
    std::memcpy(&bits,&v,sizeof(bits));
    put(name,bits);
    };

  d.desc = mem32.alloc(std::max<uint32_t>(cls->class_size(),160)+16,"oSDamageDescriptor");
  if(d.desc==0)
    return;

  const Item* weapon = attacker.inventory().activeWeapon();
  put("DWFIELDSVALID",0xFFFF);
  put("PVOBATTACKER", int32_t(ptrOf(attacker)));
  put("PNPCATTACKER", int32_t(ptrOf(attacker)));
  put("PVOBHIT", int32_t(ptrOf(victim)));
  if(weapon!=nullptr && !isSpell)
    put("PITEMWEAPON",int32_t(ptrOf(const_cast<Item&>(*weapon))));
  put("NSPELLID",isSpell ? spellId : 0);
  put("NSPELLCAT",isSpell ? gameScript.spellDesc(spellId).spell_type : 0);
  put("NSPELLLEVEL",isSpell ? 1 : 0);
  put("ENUMODEDAMAGE",isSpell ? gameScript.spellDesc(spellId).damage_type : DamageCalculator::damageTypeMask(attacker));
  putFloat("FDAMAGEMULTIPLIER",1.f);
  float total = 0.f;
  const int32_t damageOff = field("ARYDAMAGE");
  for(size_t i=0; i<zenkit::DamageType::NUM; ++i) {
    total += float(d.damage[i]);
    if(damageOff>=0)
      mem32.writeInt(d.desc+ptr32_t(damageOff)+ptr32_t(i)*4,d.damage[i]);
    }
  putFloat("FDAMAGETOTAL",total);
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
  if(damageOff>=0)
    for(size_t i=0; i<zenkit::DamageType::NUM; ++i)
      d.damage[i] = mem32.readInt(d.desc+ptr32_t(damageOff)+ptr32_t(i)*4);
  }

void DirectMemory::eventDamageEnd(Npc& victim, GameScript::DmaDamage& d) {
  if(d.desc==0)
    return;

  HookRegs r;
  r.ecx  = ptrOf(victim);
  r.args = {int32_t(d.desc)};
  fireHook(HookAdr::OnDamageScript,r);

  // CoM updates both the incoming total and ARYDAMAGE for its mage ring.
  // ARYDAMAGE is applied before protection by DamageCalculator; adding the
  // input total again here would bypass armor and double the base damage.
  if(auto* s = vm.find_symbol_by_name("OSDAMAGEDESCRIPTOR.BDAMAGEDONTKILL"))
    d.dontKill = mem32.readInt(d.desc+s->offset_as_member())!=0;
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
  const bool locked = mob->isLocked();

  if(ord==std::memory_order::release) {
    const int32_t bf = get(offset("BITFIELD"));
    const bool    nowLocked = (bf&1)!=0;
    mob->setPickLockProgress(uint32_t(bf)>>2);
    if(nowLocked!=locked && (mob->isContainer() || mob->isDoor())) {
      mob->setAsCracked(!nowLocked);
      if(!nowLocked)
        lockOpenedByScript = true;
      }
    return;
    }

  // the table of virtual functions says what it is: a chest, a door or another mob
  const int32_t vtbl = mob->isContainer() ? 8637284 : (mob->isDoor() ? 8638548 : 8639884);
  put(0,vtbl);
  put(offset("BITFIELD"),(locked ? 1 : 0) | int32_t(mob->pickLockProgress()<<2));

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

bool DirectMemory::eventSpellTarget(Interactive& mob, TargetType type) {
  if(type!=TARGET_TYPE_MOB || !hasHook(HookAdr::SpellTargetValid))
    return false;
  HookRegs regs;
  regs.ecx = uint32_t(type);
  regs.args.push_back(int32_t(mobPointer(mob)));
  return fireHook(HookAdr::SpellTargetValid,regs) && regs.ecx==1;
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
  if(key>0 && size_t(key)<sizeof(keyEvents)) {
    if(down && keyEvents[key]==0)
      keyToggles[key] = 1;
    if(!down)
      keyToggles[key] = 0;
    keyEvents[key] = down ? 1 : 0;
    }
  }

void DirectMemory::eventDialogMessage(std::string& name, uint32_t& color, bool& bottom, bool playerLine) {
  if(!hasHook(HookAdr::DialogMessage))
    return;
  const auto storage = mem32.alloc(uint32_t(sizeof(zString)+4),"dialog message hook arguments");
  if(storage==0)
    return;
  const auto colorPtr = storage+uint32_t(sizeof(zString));
  auto* text = mem32.deref<zString>(storage);
  if(text==nullptr || !memAssignString(*text,name)) {
    mem32.free(storage);
    return;
    }
  mem32.writeInt(colorPtr,int32_t(color));
  auto cleanup = [&]() {
    if(auto* value = mem32.deref<const zString>(storage))
      mem32.free(value->ptr);
    mem32.free(storage);
    };
  HookRegs regs;
  regs.ecx = memGame.ARRAY_VIEW[playerLine ? 1 : 5];
  const auto originalView = regs.ecx;
  regs.args = {int32_t(storage),0,0,int32_t(colorPtr)};
  try {
    fireHook(HookAdr::DialogMessage,regs);
    if(auto* value = mem32.deref<const zString>(storage))
      memFromString(name,*value);
    color = uint32_t(mem32.readInt(colorPtr));
    bottom = regs.ecx!=originalView && regs.ecx==memGame.ARRAY_VIEW[1];
    }
  catch(...) {
    cleanup();
    throw;
    }
  cleanup();
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

void DirectMemory::eventItemInfo(const Item& item) {
  if(!hasHook(HookAdr::ItemInfoDraw))
    return;
  const auto pointer = handles.pointerTo(item.handlePtr());
  if(pointer==0)
    return;

  withRecipeTemporary(uint32_t(std::max(item.handle().hp,0)),"OCITEMCONTAINER__DRAWITEMINFO_HOOK",[&]() {
    HookRegs regs;
    regs.args.push_back(int32_t(pointer));
    fireHook(HookAdr::ItemInfoDraw,regs);
    });
  }

// Both the recipe tooltip and CRAFTINGVIEW_SHOW create a temporary C_RECIPE.
// Its strings are copied out before returning; preserve any outer invocation
// and free the temporary rather than leaking one on each selection change.
void DirectMemory::withRecipeTemporary(uint32_t instance, std::string_view prefix, const std::function<void()>& call) {
  auto* temporary = vm.find_symbol_by_name(std::string(prefix)+".PTR");
  auto* recipeRef = vm.find_symbol_by_name(std::string(prefix)+".REC");
  auto* recipe = instance>0 ? vm.find_symbol_by_index(instance) : nullptr;
  auto* cls = recipe;
  for(size_t guard=0; cls!=nullptr && cls->type()!=zenkit::DaedalusDataType::CLASS; ++guard) {
    if(guard>=vm.symbols().size() || (cls->type()!=zenkit::DaedalusDataType::INSTANCE &&
                                    cls->type()!=zenkit::DaedalusDataType::PROTOTYPE)) {
      cls = nullptr;
      break;
      }
    cls = vm.find_symbol_by_index(cls->parent());
    }
  if(cls==nullptr || cls->name()!="C_RECIPE" || recipe==nullptr ||
     recipe->type()!=zenkit::DaedalusDataType::INSTANCE || temporary==nullptr || recipeRef==nullptr) {
    call();
    return;
    }
  const auto previousPointer = temporary->get_int();
  const auto previousInstance = recipe->get_instance();
  const auto previousRef = recipeRef->get_instance();
  temporary->set_int(0);
  auto cleanup = [&]() {
    const auto allocation = ptr32_t(temporary->get_int());
    temporary->set_int(previousPointer);
    recipeRef->set_instance(previousRef);
    recipe->set_instance(previousInstance);
    if(allocation==0)
      return;
    for(auto* member:vm.find_class_members(*cls)) {
      if(member->type()!=zenkit::DaedalusDataType::STRING)
        continue;
      for(uint32_t i=0; i<member->count(); ++i) {
        const auto address = allocation+uint32_t(member->offset_as_member())+i*uint32_t(sizeof(zString));
        if(auto* str = mem32.deref<zString>(address); str!=nullptr && str->ptr!=0)
          mem32.free(str->ptr);
        }
      }
    mem32.free(allocation);
    };
  try {
    call();
    }
  catch(...) {
    cleanup();
    throw;
    }
  cleanup();
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
void DirectMemory::eventSpellBegin(Npc& caster) {
  finishTelekinesis();
  lockOpenedByScript = false;
  if(vm.find_symbol_by_name("OCNPC.MAG_BOOK")==nullptr)
    return;
  // CoM's SPELL_CAST_SLOWDOWN reads mag_book+24 before calling SetOwner.
  // Keep a real, owned 32-bit book facade per live caster, rather than aliasing
  // a script field or returning an unmapped pointer. It is rebuilt after load.
  for(auto it=magicBooks.begin(); it!=magicBooks.end();) {
    auto owner = it->second.lock();
    if(owner!=nullptr && owner->user_ptr!=nullptr) {
      ++it;
      continue;
      }
    if(owner!=nullptr)
      owner->compat_magic_book = 0;
    mem32.free(it->first);
    it = magicBooks.erase(it);
    }
  auto ref = caster.handlePtr();
  auto book = ptr32_t(ref->compat_magic_book);
  auto it = magicBooks.find(book);
  if(it==magicBooks.end() || it->second.lock()!=ref) {
    book = mem32.alloc(64,"oCMag_Book owner/world for CoM spells");
    ref->compat_magic_book = int32_t(book);
    if(book!=0)
      magicBooks[book] = ref;
    }
  }

void DirectMemory::finishTelekinesis() {
  auto ref = telekinesisItem.lock();
  if(ref!=nullptr && ref->user_ptr!=nullptr) {
    auto* item = reinterpret_cast<Item*>(ref->user_ptr);
    if(gameScript.world().validateFocus(Focus(*item)).item==item)
      item->setPhysicsEnable(gameScript.world());
    }
  telekinesisItem.reset();
  telekinesisCaster.reset();
  if(auto* stage = vm.find_symbol_by_name("STAGE"); stage!=nullptr && stage->type()==zenkit::DaedalusDataType::INT)
    stage->set_int(0);
  }

void DirectMemory::tickTelekinesis() {
  auto item = telekinesisItem.lock();
  if(item==nullptr)
    return;
  auto caster = telekinesisCaster.lock();
  if(item->user_ptr==nullptr || caster==nullptr || caster->user_ptr==nullptr ||
     !reinterpret_cast<Npc*>(caster->user_ptr)->isCasting())
    finishTelekinesis();
  }

void DirectMemory::setupTelekinesisFunctions() {
  auto* logic = vm.find_symbol_by_name("SPELL_LOGIC_TELEKINESIS");
  if(logic==nullptr || vm.find_symbol_by_name("OCITEM_TELEKINESIS")==nullptr)
    return;
  auto integer = [this](std::string_view name) {
    auto* s = vm.find_symbol_by_name(name);
    return s!=nullptr && s->type()==zenkit::DaedalusDataType::INT ? s->get_int() : 0;
    };
  auto set = [this](std::string_view name, int32_t value) {
    if(auto* s = vm.find_symbol_by_name(name); s!=nullptr && s->type()==zenkit::DaedalusDataType::INT)
      s->set_int(value);
    };
  auto bits = [](float value) {
    int32_t result = 0;
    std::memcpy(&result,&value,sizeof(result));
    return result;
    };
  auto move = [this,integer]() {
    auto ref = telekinesisItem.lock();
    if(ref==nullptr || ref->user_ptr==nullptr)
      return;
    auto* item = reinterpret_cast<Item*>(ref->user_ptr);
    const Vec3 delta(bitsToFloat(integer("STEP_X")),bitsToFloat(integer("STEP_Z")),bitsToFloat(integer("STEP_Y")));
    const auto to = item->position()+delta;
    if(!std::isfinite(to.x) || !std::isfinite(to.y) || !std::isfinite(to.z)) {
      finishTelekinesis();
      return;
      }
    item->setPosition(to.x,to.y,to.z);
    };
  vm.override_function("NEW_TELEKINESIS",move);
  vm.override_function("SPELL_LOGIC_TELEKINESIS",[this,integer,set,bits,move](int invested) -> int {
    auto casterRef = std::dynamic_pointer_cast<zenkit::INpc>(vm.global_self()->get_instance());
    auto* caster = casterRef==nullptr ? nullptr : reinterpret_cast<Npc*>(casterRef->user_ptr);
    if(caster==nullptr)
      return SPL_DONTINVEST;
    int stage = integer("STAGE");
    const int cost = std::max(integer("STEP_TELEKINESIS"),1);
    auto ref = telekinesisItem.lock();
    if(stage==0 || ref==nullptr) {
      auto selected = handles.instanceAt(ptr32_t(casterRef->compat_focus_vob));
      ref = std::dynamic_pointer_cast<zenkit::IItem>(selected);
      auto* item = ref==nullptr ? nullptr : reinterpret_cast<Item*>(ref->user_ptr);
      if(item==nullptr || gameScript.world().validateFocus(Focus(*item)).item!=item)
        return SPL_DONTINVEST;
      const auto delta = caster->position()-item->position();
      const float distance = delta.length();
      if(!std::isfinite(distance) || distance<180.f || distance>10000.f)
        return SPL_DONTINVEST;
      const int steps = std::max(int(distance)/10,1);
      set("NUMB_OF_STEPS",steps);
      set("STEP_X",bits(delta.x/float(steps)));
      set("STEP_Z",bits(delta.y/float(steps)));
      set("STEP_Y",bits(delta.z/float(steps)));
      vm.find_symbol_by_name("OCITEM_TELEKINESIS")->set_instance(ref);
      stage = 0;
      set("STAGE",0);
      }
    auto* item = ref==nullptr ? nullptr : reinterpret_cast<Item*>(ref->user_ptr);
    if(item==nullptr || caster->attribute(ATR_MANA)<cost) {
      finishTelekinesis();
      return SPL_DONTINVEST;
      }
    if(invested<=cost)
      return SPL_STATUS_CANINVEST_NO_MANADEC;
    if((stage-1)%10==0)
      casterRef->attribute[ATR_MANA] = std::max(casterRef->attribute[ATR_MANA]-cost,0);
    if((caster->position()-item->position()).length()<150.f)
      return SPL_SENDCAST;
    if(stage==0) {
      telekinesisItem = ref;
      telekinesisCaster = casterRef;
      item->setPhysicsDisable();
      auto at = item->position();
      if(const auto* bounds = item->bBox()) {
        float minY = std::numeric_limits<float>::max(), maxY = std::numeric_limits<float>::lowest();
        for(unsigned i=0; i<8; ++i) {
          Vec3 point(bounds[(i&1)!=0].x,bounds[(i&2)!=0].y,bounds[(i&4)!=0].z);
          item->transform().project(point);
          minY = std::min(minY,point.y);
          maxY = std::max(maxY,point.y);
          }
        if(std::isfinite(minY) && std::isfinite(maxY))
          at.y += maxY-minY;
        }
      item->setPosition(at.x,at.y,at.z);
      if(const auto* fx = Gothic::inst().loadVisualFx("spellFX_ItemAusbuddeln")) {
        Effect effect(*fx,gameScript.world(),at);
        effect.setActive(true);
        gameScript.world().runEffect(std::move(effect));
        }
      }
    move();
    set("STAGE",stage<INT32_MAX ? stage+1 : stage);
    return SPL_NEXTLEVEL;
    });
  if(auto* cast = vm.find_symbol_by_name("SPELL_CAST_TELEKINESIS"); cast!=nullptr && cast->count()==0)
    vm.override_function("SPELL_CAST_TELEKINESIS",[this]() {
      finishTelekinesis();
      auto ref = std::dynamic_pointer_cast<zenkit::INpc>(vm.global_self()->get_instance());
      if(ref!=nullptr && ref->aivar[20]<INT32_MAX)
        ++ref->aivar[20];
      });
  vm.override_function("SPELL_TELEKINESIS_FOCUS_REMOVER",[this]() -> int {
    finishTelekinesis();
    auto hero = std::dynamic_pointer_cast<zenkit::INpc>(vm.global_hero()->get_instance());
    if(hero!=nullptr)
      hero->compat_focus_vob = 0;
    return 0;
    });
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

static bool equalsNoCase(std::string_view a, std::string_view b) {
  if(a.size()!=b.size())
    return false;
  for(size_t i=0; i<a.size(); ++i)
    if(std::toupper(static_cast<unsigned char>(a[i]))!=std::toupper(static_cast<unsigned char>(b[i])))
      return false;
  return true;
  }

/// A character falls unconscious (oCNpc::DropUnconscious). OCNPC__DROPUNCONSCIOUS_HOOK of CoM (read from the DAT): in the
/// fights of the arena, in Q205 and in Q306 the hero does not drop the weapon from his hands, the hook takes it out of the
/// hands (RemoveFromSlot) and puts it back. True means that the weapon stays.
bool DirectMemory::eventDropUnconscious(Npc& npc) {
  if(!hasHook(HookAdr::DropUnconscious))
    return false;
  HookRegs r;
  r.esi = ptrOf(npc);
  inDropUnconscious = true;
  keepHandWeapons   = false;
  fireHook(HookAdr::DropUnconscious,r);
  inDropUnconscious = false;
  const bool keep = keepHandWeapons;
  keepHandWeapons = false;
  return keep;
  }

/// CoM replaces auto-equip with _FIXEQUIPBESTWEAPONS, which reads the NPC from ESI.
bool DirectMemory::eventEquipBestWeapons(Npc& npc) {
  if(!hasHook(HookAdr::EquipBestWeapons))
    return false;
  HookRegs regs;
  regs.esi = ptrOf(npc);
  if(regs.esi==0)
    return false;
  return fireHook(HookAdr::EquipBestWeapons,regs);
  }

bool DirectMemory::eventEnemyCandidate(Npc& npc, Npc& candidate) {
  if(!hasHook(HookAdr::NextEnemy))
    return true;
  // Recheck cached targets as well: a quest may have just changed their
  // attitude or added them to the party since the previous perception tick.
  if(!npc.isEnemy(candidate))
    return false;
  HookRegs regs;
  regs.ebp = ptrOf(npc);
  regs.edi = ptrOf(candidate);
  if(regs.ebp==0 || regs.edi==0)
    return false;
  // DAT: party/party -> ECX=1; otherwise NPC_CanSeeNpc decides 0/1.
  // The original instruction following the replaced block skips nonzero ECX.
  return fireHook(HookAdr::NextEnemy,regs) && regs.ecx==0;
  }

/// A character equips or unequips a bow or a crossbow (oCNpc::EquipItem, oCNpc::UnequipItem). The hooks _EVT_QUIVER_EQUIP and
/// _EVT_QUIVER_UNEQUIP of CoM (read from the DAT) hang the quiver on the back of the character or take it down.
void DirectMemory::eventEquipItem(Npc& npc, Item& item, bool equip) {
  const uint32_t adr = equip ? HookAdr::EquipItem : HookAdr::UnequipItem;
  if(!hasHook(adr))
    return;
  HookRegs r;
  r.ecx = ptrOf(npc);
  r.args.push_back(int32_t(ptrOf(item)));
  fireHook(adr,r);
  }

/// MEM_InsertVob, oCNpc::PutInSlot and oCNpc::RemoveFromSlot as the quivers of CoM use them (REFRESHBOWQUIVERMODEL, REMOVEQUIVER):
/// a vob with the visual "ItRw_Quiver_<arrows>.3ds" is put in the slot ZS_CROSSBOW (for a bow) or ZS_BOW (for a crossbow) of the
/// character, then the script deletes the vob from the world, the model keeps its reference. Here the vob is only the name of
/// the visual until it is put in a slot; the slot is a decoration of the model.
void DirectMemory::setupQuiverFunctions() {
  auto safe = [&](std::string_view name, auto hook) {
    auto f = vm.find_symbol_by_name(name);
    if(f==nullptr || f->type()!=zenkit::DaedalusDataType::FUNCTION)
      return;
    vm.override_function(name,hook);
    };

  auto npcOf = [](const std::shared_ptr<zenkit::INpc>& ref) -> Npc* {
    if(ref==nullptr || ref->user_ptr==nullptr)
      return nullptr;
    return reinterpret_cast<Npc*>(ref->user_ptr);
    };
  auto quiverSlot = [](std::string_view slot) {
    return equalsNoCase(slot,"ZS_BOW") || equalsNoCase(slot,"ZS_CROSSBOW");
    };

  safe("TELEPORTHEROTOPOS", [this](int x, int y, int z) {
    auto* hero = gameScript.world().player();
    const Vec3 pos(bitsToFloat(x),bitsToFloat(y),bitsToFloat(z));
    if(hero==nullptr || !std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z))
      return;
    hero->clearGoTo();
    hero->setPosition(pos);
    });

  safe("BUFF_DIVE10_TICK", [this](int buff) {
    // Retain LeGo's handle -> saved target ID lookup. Only the two native
    // oCNpc float members written by the original callback need an adapter.
    auto* getNpc = vm.find_symbol_by_name("BUFF_GETNPC");
    if(getNpc==nullptr || getNpc->type()!=zenkit::DaedalusDataType::FUNCTION)
      return;
    const int ptr = vm.call_function<int>("BUFF_GETNPC",buff);
    auto ref = handles.instanceAt(ptr32_t(ptr));
    auto* npc = dynamic_cast<zenkit::INpc*>(ref.get());
    if(npc!=nullptr && npc->user_ptr!=nullptr)
      reinterpret_cast<Npc*>(npc->user_ptr)->setDiveBreath(30000,31000);
    });

  // AI_TeleportAlways of LeGo: the command that the queue calls writes the matrix of the vob of the character through zCVob, which the
  // layer does not have as a class of the scripts (illegal access of unbound member ZCVOB.TRAFOOBJTOWORLD). The character is put at the
  // point, the same way as TeleportNpcToWP does it.
  safe("_AI_TELEPORTALWAYS", [this](int npcPtr, std::string_view wpName){
    auto inst = handles.instanceAt(InstanceHandles::ptr32_t(npcPtr));
    auto* inpc = dynamic_cast<zenkit::INpc*>(inst.get());
    auto* wp = gameScript.world().findPoint(wpName,false);
    if(inpc==nullptr || inpc->user_ptr==nullptr || wp==nullptr) {
      Log::d("AI_TeleportAlways: the character or the way point is not there: ",wpName);
      return;
      }
    auto& npc = *reinterpret_cast<Npc*>(inpc->user_ptr);
    auto  pos = wp->position();
    const float drop = pos.y-wp->groundPos.y;
    if(drop>0.f && drop<200.f)
      pos = wp->groundPos;
    npc.clearGoTo();
    npc.setPosition(pos);
    npc.setDirection(wp->direction());
    });

  // CoM changes the armor of a character with Mdl_SetVisualBody(npc, npc.body_visualName, npc.bitfield..., armor): it reads the visual
  // of the character from the members of oCNpc that the VM does not have. The visual stays, the armor is put on.
  safe("NPC_CHANGEARMOR", [npcOf](std::shared_ptr<zenkit::INpc> slf, int armor){
    auto* npc = npcOf(slf);
    if(npc!=nullptr) {
      if(armor>0) {
        if(npc->itemCount(uint32_t(armor))==0)
          npc->addItem(uint32_t(armor),1);
        npc->equipItem(uint32_t(armor));
        }
      else if(armor<0) {
        if(auto* equipped = npc->currentArmor())
          npc->unequipItem(equipped->clsId());
        }
      Log::d("Npc_ChangeArmor: ",npc->displayName());
      }
    // The original CoM body records the new armor even for NO_ARMOR (-1).
    // Refresh/reinsert scripts rely on this, beyond the visible model change.
    if(slf!=nullptr)
      slf->aivar[48] = armor;
    });

  safe("MEM_GETANYWP", [](){
    return std::string("WP");
    });
  safe("MEM_INSERTVOB", [this](std::string_view visual, std::string_view /*wp*/) -> int {
    const ptr32_t vob = mem32.alloc(16,"zCVob");
    if(vob!=0)
      decoVobs[vob] = std::string(visual);
    return int(vob);
    });
  safe("MEM_DELETEVOB", [this](int vob) {
    auto it = decoVobs.find(ptr32_t(vob));
    if(it==decoVobs.end())
      return;
    decoVobs.erase(it);
    mem32.free(ptr32_t(vob));
    });
  safe("OCNPC_PUTINSLOT", [this,npcOf,quiverSlot](std::shared_ptr<zenkit::INpc> npcRef, std::string_view slot, int vob, int /*slotId*/) -> int {
    auto it = decoVobs.find(ptr32_t(vob));
    auto* npc = npcOf(npcRef);
    if(npc==nullptr || it==decoVobs.end() || !quiverSlot(slot))
      return 0;
    auto mesh = npc->world().addView(it->second);
    npc->setDecor(std::move(mesh),slot);
    return 1;
    });
  safe("OCNPC_REMOVEFROMSLOT", [this,npcOf,quiverSlot](std::shared_ptr<zenkit::INpc> npcRef, std::string_view slot, int /*drop*/, int /*stopEffect*/) -> int {
    auto* npc = npcOf(npcRef);
    if(npc==nullptr)
      return 0;
    if(quiverSlot(slot)) {
      npc->clearDecor(slot);
      return 0;
      }
    const bool left = equalsNoCase(slot,"ZS_LEFTHAND");
    const bool right = equalsNoCase(slot,"ZS_RIGHTHAND");
    if(inDropUnconscious && (right || left)) {
      // CoM immediately re-equips the returned weapon in protected fights.
      // Keep its inventory object alive rather than spawning a loose copy.
      keepHandWeapons = true;
      if(auto* item = npc->itemInSlot(slot))
        return int(handles.pointerTo(item->handlePtr()));
      }
    if(left && npc->isUsingTorch()) {
      const auto result = handles.pointerTo(npc->torchItem());
      // TORCH_UNEQUIP creates the unlit inventory torch itself. Removing the
      // burning visual must not spawn another torch or grant an extra item.
      npc->setTorch(false);
      return int(result);
      }
    return 0;
    });

  cpu.register_thiscall(7566816,[](ptr32_t) {});   // oCNpc::DropAllInHand: OpenGothic drops the weapon itself (Npc::onNoHealth)

  // ENABLESAVEGAME and ENABLELOADSAVEGAME of CoM switch the items of the menu of the original engine: the scenes do not let the
  // player save (the camera of a cutscene, the final boards)
  safe("ENABLESAVEGAME",     [this](int enable){ saveOff = enable==0; });
  safe("ENABLELOADSAVEGAME", [this](int enable){ saveOff = loadOff = enable==0; });
  safe("ENABLELOADGAME",     [this](int enable){ loadOff = enable==0; });

  // DIACAM_DISABLE and DIACAM_ENABLE of CoM write a return into the first instruction of zCAICamera::StartDialogCam: the scenes
  // with their own cameras (zCCSCamera) have no camera of the dialog that fights with them
  safe("DIACAM_DISABLE", [this](){ dialogCameraOff = true;  });
  safe("DIACAM_ENABLE",  [this](){ dialogCameraOff = false; });
  safe("DIACAM_UPDATE", [this]() {
    // _TRIA_Uncam requests this before sending the camera's untrigger. The
    // main window applies it once the cutscene has actually released control.
    if(!dialogCameraOff && Gothic::inst().isInDialog())
      Gothic::inst().onDialogCameraReset();
    });

  // CHANGEVOBCOLLISION(name, staticVob, collStat, collDyn, showVisual) writes the bits of zCVob.bitfield0 of the vobs of the
  // name: the scripts of CoM hide and show the decorations of the quests and switch their collision
  safe("CHANGEVOBCOLLISION", [this](std::string_view name, int /*staticVob*/, int /*collStat*/, int collDyn, int showVisual) {
    auto& w = gameScript.world();
    std::string key(w.name());
    key += '|';
    for(char c:name)
      key += char(std::toupper(static_cast<unsigned char>(c)));
    vobStates[key] = VobState{showVisual!=0,collDyn!=0};
    w.setVobVisual(name,showVisual!=0,collDyn!=0);
    });

  // the quivers of the characters of the world: the scripts walk through oWorld.voblist_npcs, here the hook of equipping is
  // given to every character with a bow, a frame later (the characters of the world exist then)
  safe("INIT_QUIVERS_ALWAYS", [this](){
    quiversPending = true;
    });
  }

/// The states of the vobs that the scripts have changed are applied again to the world that was loaded (a savegame, a change of
/// the level): the vobs of the world are made from the ZEN file with the state of the file
void DirectMemory::tickVobStates() {
  if(!vobStatesPending)
    return;
  auto& w = gameScript.world();
  if(w.npcCount()==0)
    return;
  vobStatesPending = false;

  const std::string prefix = std::string(w.name())+"|";
  for(auto& i:vobStates) {
    if(i.first.compare(0,prefix.size(),prefix)!=0)
      continue;
    w.setVobVisual(std::string_view(i.first).substr(prefix.size()),i.second.show,i.second.collide);
    }
  }

void DirectMemory::tickQuivers() {
  if(!quiversPending)
    return;
  auto& world = gameScript.world();
  if(world.npcCount()==0)
    return;
  quiversPending = false;
  for(uint32_t i=0, n=world.npcCount(); i<n; ++i) {
    auto* npc = world.npcById(i);
    if(npc==nullptr || npc->handlePtr()==nullptr)
      continue;
    if(auto* bow = npc->currentRangedWeapon())
      eventEquipItem(*npc,*bow,true);
    }
  }

/// Trialoge of LeGo (TRIA_Invite, TRIA_Start, TRIA_Next, TRIA_Finish: the talks of three and more characters, about 1300 calls in
/// CoM). The library lets one character, the self of the dialog, speak for all of them: at every TRIA_Next it swaps the name, the
/// voice, the visual and the place in the memory of the objects of the engine, which is not here. Here the commands that the
/// script gives to the self go to the character who speaks now (GameScript::findNpc), and the dialog accepts his lines.
auto DirectMemory::triaRedirect(Npc* npc) -> Npc* {
  if(triaCurrent==nullptr || npc==nullptr || npc!=triaOriginal)
    return npc;
  auto* running = vm.find_symbol_by_name("TRIA_RUNNING");
  if(running==nullptr || running->get_int()==0)
    return npc;
  return triaCurrent;
  }

bool DirectMemory::trialogFinishPending() const {
  auto* running = vm.find_symbol_by_name("TRIA_RUNNING");
  return triaFinishing && running!=nullptr && running->get_int()!=0;
  }

void DirectMemory::setupTrialogFunctions() {
  auto safe = [&](std::string_view name, auto hook) {
    auto f = vm.find_symbol_by_name(name);
    if(f==nullptr || f->type()!=zenkit::DaedalusDataType::FUNCTION)
      return;
    vm.override_function(name,hook);
    };
  if(vm.find_symbol_by_name("TRIA_NEXT")==nullptr)
    return;

  auto intVar = [this](std::string_view name) -> int {
    auto* s = vm.find_symbol_by_name(name);
    return s!=nullptr && s->type()==zenkit::DaedalusDataType::INT ? s->get_int() : 0;
    };
  auto npcAt = [this](ptr32_t ptr) -> Npc* {
    auto inst = handles.instanceAt(ptr);
    if(inst==nullptr || dynamic_cast<zenkit::INpc*>(inst.get())==nullptr || inst->user_ptr==nullptr)
      return nullptr;
    return reinterpret_cast<Npc*>(inst->user_ptr);
    };

  // the swaps of the memory are not needed
  safe("_TRIA_COPY",         [](int, int){});
  safe("_TRIA_COPYNPC",      [](int){});
  safe("_TRIA_NEXT",         [](int){});
  safe("_TRIA_UPDATEVISUAL", [](std::shared_ptr<zenkit::INpc>, int){});

  safe("TRIA_NEXT", [=,this](std::shared_ptr<zenkit::INpc> ref) {
    if(intVar("TRIA_RUNNING")==0 || ref==nullptr || ref->user_ptr==nullptr)
      return;
    auto* next = reinterpret_cast<Npc*>(ref->user_ptr);
    auto* hero = gameScript.world().player();
    auto* self = npcAt(ptr32_t(intVar("TRIA_SELF")));
    if(next==nullptr || hero==nullptr || self==nullptr || next==hero)
      return;
    if(triaOriginal!=self) {
      triaOriginal = self;
      triaCurrent  = self;
      }

    // what was said goes first (TRIA_Wait), then the next one begins
    auto* cur = triaCurrent;
    hero->aiWaitTillEnd(*cur);
    cur ->aiWaitTillEnd(*hero);
    hero->aiWaitTillEnd(*cur);
    if(next!=cur)
      next->aiWaitTillEnd(*hero);
    triaCurrent = next;
    if(next!=self)
      Gothic::inst().onDialogGuest(*next);
    });

  safe("TRIA_FINISH", [this,intVar,npcAt]() {
    auto* original = vm.find_symbol_by_name("TRIA_FINISH");
    auto* hero = gameScript.world().player();
    const bool running = intVar("TRIA_RUNNING")!=0;
    std::vector<Npc*> participants;
    auto add = [&](Npc* npc) {
      if(npc!=nullptr && npc!=hero && std::find(participants.begin(),participants.end(),npc)==participants.end())
        participants.push_back(npc);
      };
    add(triaOriginal);
    add(triaCurrent);
    add(npcAt(ptr32_t(intVar("TRIA_SELF"))));
    if(auto* pointers = vm.find_symbol_by_name("TRIA_NPCPTR")) {
      const auto count = std::clamp(intVar("TRIA_CPTR"),0,int(pointers->count()));
      for(int i=0; i<count; ++i)
        add(npcAt(ptr32_t(pointers->get_int(uint16_t(i)))));
      }

    // Run the original no-argument bytecode, deliberately bypassing this
    // override: it keeps LeGo's validation, TRIA_Wait and camera untrigger,
    // and queues _TRIA_Finish on the hero.
    vm.unsafe_call(original);
    if(!running || hero==nullptr)
      return;
    triaFinishing = true;

    // Logical speakers have separate queues here; LeGo swaps their original
    // engine object in place. Commands queued after TRIA_Finish (notably
    // Q102_ChangeRoutine and AI_StopProcessInfos) must wait for the hero's
    // queued cleanup, rather than ending ZS_TALK with TRIA_RUNNING still 1.
    for(auto* npc:participants)
      npc->aiWaitTillEnd(*hero);
    if(logBudget("tria-finish-sync"))
      Log::d("TRIA_Finish: queued completion before post-dialog commands for ",participants.size()," participants");
    });

  safe("_TRIA_FINISH", [this,intVar,npcAt]() {
    if(auto* hero = gameScript.world().player())
      hero->aiPush(AiQueue::aiStopLookAt());
    // CoM's embedded _TRIA_Finish restores invited NPCs' AIV_INVINCIBLE.
    // The last table entry is the original SELF, which was not initialized as
    // a guest and must retain its own flag.
    if(auto* pointers = vm.find_symbol_by_name("TRIA_NPCPTR")) {
      auto* invincible = vm.find_symbol_by_name("AIV_INVINCIBLE");
      const int flag = invincible==nullptr ? -1 : invincible->get_int();
      const auto count = std::clamp(intVar("TRIA_CPTR")-1,0,int(pointers->count()));
      for(int i=0; i<count; ++i) {
        if(auto* npc = npcAt(ptr32_t(pointers->get_int(uint16_t(i))))) {
          npc->aiPush(AiQueue::aiStopLookAt());
          if(flag>=0 && size_t(flag)<std::size(npc->handle().aivar))
            npc->handle().aivar[flag] = 0;
          }
        }
      }
    if(auto* s = vm.find_symbol_by_name("TRIA_RUNNING"))
      s->set_int(0);
    if(auto* s = vm.find_symbol_by_name("TRIA_CPTR"))
      s->set_int(0);
    triaCurrent  = nullptr;
    triaOriginal = nullptr;
    triaFinishing = false;
    });
  }
