#include "quickslots.h"

#include "gothic.h"
#include "world/objects/item.h"
#include "world/objects/npc.h"
#include "world/world.h"
#include "utils/string_frm.h"
#include "utils/quickslotinput.h"

#include <algorithm>
#include <sstream>

static std::vector<std::string> pinnedNames() {
  std::vector<std::string> names;
  std::istringstream in{std::string(Gothic::settingsGetS("QuickSlots","Wheel"))};
  for(std::string name; in>>name;)
    names.push_back(std::move(name));
  return names;
  }

QuickSlots::Kind QuickSlots::kind(const Item& item) {
  if((item.mainFlag()&(ITM_CAT_NF|ITM_CAT_FF))!=0) return Kind::Weapons;
  if(item.isSpellOrRune()) return Kind::Magic;
  if((item.mainFlag()&ITM_CAT_POTION)!=0) return Kind::Potions;
  if((item.mainFlag()&ITM_CAT_FOOD)!=0) return Kind::Food;
  if((item.mainFlag()&ITM_CAT_DOCS)!=0) return Kind::Documents;
  return Kind::Empty;
  }

QuickSlots::Kind QuickSlots::kind(size_t slot) {
  if(slot>=4 || Gothic::settingsGetS("QuickSlots",directions[slot]).empty()) return Kind::Empty;
  const auto category=Gothic::settingsGetS("QuickSlots",string_frm(directions[slot],"Kind"));
  for(auto k:{Kind::Weapons,Kind::Magic,Kind::Potions,Kind::Food,Kind::Documents})
    if(category==title(k)) return k;
  return Kind::Empty;
  }

std::string_view QuickSlots::title(Kind kind) {
  switch(kind) {
    case Kind::Weapons: return "Weapons";
    case Kind::Magic: return "Magic";
    case Kind::Potions: return "Potions";
    case Kind::Food: return "Food";
    case Kind::Documents: return "Maps and documents";
    default: return "Empty";
    }
  }

bool QuickSlots::assign(Npc& player,size_t slot,const Item& item) {
  if(slot>=4 || kind(item)==Kind::Empty || !item.checkCond(player) || player.itemCount(item.clsId())==0) return false;
  auto symbol=player.world().script().findSymbol(item.clsId());
  if(symbol==nullptr) return false;
  Gothic::settingsSetS("QuickSlots",directions[slot],symbol->name());
  Gothic::settingsSetS("QuickSlots",string_frm(directions[slot],"Kind"),title(kind(item)));
  Gothic::flushSettings();
  return true;
  }

Item* QuickSlots::resolve(Npc& player,size_t slot) {
  if(kind(slot)==Kind::Empty) return nullptr;
  auto& script=player.world().script();
  const auto name=Gothic::settingsGetS("QuickSlots",directions[slot]);
  if(auto item=player.getItem(script.findSymbolIndex(name)); item!=nullptr && item->count()>0) return item;

  // Only ordinary healing/mana potions substitute automatically, never permanent bonuses.
  const auto family=QuickSlotInput::potionFamily(name);
  if(family.empty()) return nullptr;
  for(auto tier:{"01","02","03"}) {
    if(auto item=player.getItem(script.findSymbolIndex(string_frm(family,tier))); item!=nullptr && item->count()>0 &&
       kind(*item)==Kind::Potions && item->checkCond(player)) {
      assign(player,slot,*item);
      return item;
      }
    }
  return nullptr;
  }

bool QuickSlots::canPin(const Item& item) {
  const auto k = kind(item);
  return k==Kind::Potions || k==Kind::Food || k==Kind::Documents;
  }

bool QuickSlots::isPinned(Npc& player, const Item& item) {
  auto symbol = player.world().script().findSymbol(item.clsId());
  if(symbol==nullptr) return false;
  const auto names = pinnedNames();
  return std::find(names.begin(),names.end(),symbol->name())!=names.end();
  }

bool QuickSlots::togglePinned(Npc& player, const Item& item) {
  auto symbol = player.world().script().findSymbol(item.clsId());
  if(symbol==nullptr || !canPin(item)) return false;
  auto names = pinnedNames();
  if(auto it=std::find(names.begin(),names.end(),symbol->name()); it!=names.end())
    names.erase(it); else
    names.push_back(symbol->name());
  std::string value;
  for(auto& name:names)
    value += (value.empty() ? "" : " ")+name;
  Gothic::settingsSetS("QuickSlots","Wheel",value);
  Gothic::flushSettings();
  return true;
  }

std::vector<size_t> QuickSlots::pinned(Npc& player) {
  std::vector<size_t> ret;
  auto& script = player.world().script();
  for(auto& name:pinnedNames()) {
    const size_t id = script.findSymbolIndex(name);
    if(auto item=player.getItem(id); item!=nullptr && item->count()>0 && item->checkCond(player))
      ret.push_back(id);
    }
  return ret;
  }
