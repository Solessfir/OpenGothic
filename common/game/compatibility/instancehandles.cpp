#include "instancehandles.h"

#include <cstring>

#include <zenkit/addon/daedalus.hh>

namespace {
// Gothic 2: the tables of virtual functions, see oCNpc_vtbl and oCItem_vtbl of Ikarus
constexpr int32_t OCNPC_VTBL  = 8640292;
constexpr int32_t OCITEM_VTBL = 8636420;
}

InstanceHandles::InstanceHandles(Mem32& mem) : mem(mem) {
  }

void InstanceHandles::setup() {
  mem.setCallbackR(Mem32::Type::instanceHandle,[this](Slot& s, uint32_t i) {
    fill(s,i);
    });
  // the scripts do not change a character through its handle
  mem.setCallbackW(Mem32::Type::instanceHandle,[this](Slot& s, uint32_t i) {
    if(sync && i<entries.size() && entries[i].inst!=nullptr)
      sync(s.raw,*entries[i].inst,true);
    });

  entries.resize(SlotCount);
  base = mem.alloc(SlotCount*SlotSize,Mem32::Type::instanceHandle,"oCNpc, oCItem");
  }

void InstanceHandles::fill(Slot& s, uint32_t i) {
  // the rest of the object reads as zeroes
  const int32_t vtbl = (i<entries.size() && entries[i].inst!=nullptr) ? entries[i].vtbl : 0;
  std::memcpy(s.raw,&vtbl,sizeof(int32_t));
  if(sync && i<entries.size() && entries[i].inst!=nullptr)
    sync(s.raw,*entries[i].inst,false);
  }

auto InstanceHandles::pointerTo(const std::shared_ptr<zenkit::DaedalusInstance>& inst) -> ptr32_t {
  if(inst==nullptr || base==0)
    return 0;

  auto it = index.find(inst.get());
  if(it!=index.end())
    return base + it->second*SlotSize;

  int32_t vtbl = 0;
  if(dynamic_cast<zenkit::INpc*>(inst.get())!=nullptr)
    vtbl = OCNPC_VTBL;
  else if(dynamic_cast<zenkit::IItem*>(inst.get())!=nullptr)
    vtbl = OCITEM_VTBL;
  else
    return 0;

  // the slots are used in turns: the oldest one is given away when all are taken
  const uint32_t i = cursor;
  cursor = (cursor+1) % SlotCount;
  if(entries[i].inst!=nullptr)
    index.erase(entries[i].inst.get());
  else
    ++used;
  entries[i].inst = inst;
  entries[i].vtbl = vtbl;
  index[inst.get()] = i;
  return base + i*SlotSize;
  }

bool InstanceHandles::contains(ptr32_t address) const {
  return base!=0 && base<=address && address<base+SlotCount*SlotSize;
  }

auto InstanceHandles::instanceAt(ptr32_t address) const -> std::shared_ptr<zenkit::DaedalusInstance> {
  if(!contains(address))
    return nullptr;
  const uint32_t i = (address-base)/SlotSize;
  return entries[i].inst;
  }
