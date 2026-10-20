#pragma once

#include "graphics/objvisual.h"
#include "vob.h"

class StaticObj : public Vob {
  public:
    StaticObj(Vob* parent, World& world, const zenkit::VirtualObject& vob, Flags flags);

    void  setVobVisual(bool show, bool collideDynamic) override;

  private:
    void  moveEvent() override;
    bool  setMobState(std::string_view scheme,int32_t st) override;

    ObjVisual   visual;
    std::string scheme;
  };

