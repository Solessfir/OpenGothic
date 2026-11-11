#include "globalfx.h"
#include "npc.h"

GlobalFx::GlobalFx() {
  }

GlobalFx::GlobalFx(const std::shared_ptr<GlobalEffects::Effect>& h)
  :h(h) {
  }

GlobalFx::GlobalFx(GlobalFx&& other)
  :h(other.h) {
  other.h = nullptr;
  }

GlobalFx& GlobalFx::operator =(GlobalFx&& other) {
  std::swap(h,other.h);
  return *this;
  }

GlobalFx::~GlobalFx() {
  if(h!=nullptr)
    h->stop();
  }

void GlobalFx::setOrigin(Npc* npc) {
  auto effect = std::dynamic_pointer_cast<GlobalEffects::SlowTime>(h);
  if(effect==nullptr)
    return;
  if(npc!=nullptr)
    effect->origin = npc->handlePtr(); else
    effect->origin.reset();
  }

uint64_t GlobalFx::effectPrefferedTime() const {
  if(h==nullptr)
    return 0;
  uint64_t ret = h->timeLen;
  if(ret==uint64_t(-1))
    return h->timeLoop;
  return ret;
  }
