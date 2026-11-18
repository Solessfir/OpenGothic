#include "triggerscript.h"

#include <zenkit/vobs/Trigger.hh>
#include <Tempest/Log>

#include "world/world.h"

TriggerScript::TriggerScript(Vob* parent, World &world, const zenkit::VTriggerScript& data, Flags flags)
  :AbstractTrigger(parent,world,data,flags) {
  function = data.function;
  }

void TriggerScript::onTrigger(const TriggerEvent &) {
  try {
    // Trigger events don't carry the character that set them off; in practice it is the player walking into a trigger
    world.script().invokeTriggerScript(function,world.player());
    }
  catch(const std::exception& e){
    Tempest::Log::e("exception in trigger-script: ",e.what());
    }
  }
