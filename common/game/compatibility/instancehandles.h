#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include <zenkit/DaedalusScript.hh>

#include "game/compatibility/mem32.h"

/// The pointers that scripts of Ikarus get for the characters and the items (MEM_InstToPtr, _@).
///
/// The characters and the items are objects of the VM (C_NPC, C_ITEM), not memory of the original engine. Every one of
/// them gets its own address in the pretended memory, so that the scripts can keep the pointer, compare it, put it in
/// a list and turn it back into the instance (MEM_PtrToInst). What lives at the address is only what the scripts check
/// before they trust a pointer: the first word is the table of virtual functions of oCNpc or oCItem.
class InstanceHandles {
  public:
    using ptr32_t = Mem32::ptr32_t;

    explicit InstanceHandles(Mem32& mem);

    void     setup();

    /// What lives in the memory of the object besides the table of virtual functions: the matrix of the vob, ...
    using Sync = std::function<void(uint8_t* raw, zenkit::DaedalusInstance& inst, bool write)>;
    void     setSync(Sync f) { sync = std::move(f); }

    /// 0 for an instance that is not a character and not an item
    auto     pointerTo(const std::shared_ptr<zenkit::DaedalusInstance>& inst) -> ptr32_t;
    /// null if the address is not a handle
    auto     instanceAt(ptr32_t address) const -> std::shared_ptr<zenkit::DaedalusInstance>;
    bool     contains(ptr32_t address) const;

    size_t   count() const { return used; }

  private:
    static constexpr uint32_t SlotCount = 4096;
    static constexpr uint32_t SlotSize  = 2576;   // oCNpc is 2564 bytes

    struct Slot {
      uint8_t raw[SlotSize] = {};
      };

    void     fill(Slot& s, uint32_t index);

    Mem32&   mem;
    Sync     sync;
    ptr32_t  base   = 0;
    uint32_t cursor = 0;
    size_t   used   = 0;

    struct Entry {
      std::shared_ptr<zenkit::DaedalusInstance> inst;
      int32_t                                   vtbl = 0;
      };
    std::vector<Entry>                                    entries;
    std::unordered_map<const zenkit::DaedalusInstance*,uint32_t> index;
  };
