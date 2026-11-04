#pragma once

#include <cmath>
#include <cstdint>

// Notices the hero trying to move for a long time without leaving a small area,
// so places where the level traps him show up in the log of player reports.
class StuckWatch {
  public:
    struct Pos { float x=0, y=0, z=0; };

    static constexpr float    Radius = 150.f;  // staying inside this circle counts as not getting anywhere
    static constexpr uint64_t TryMs  = 8000;   // of trying to move inside it

    // True once the hero tried to get out for TryMs without leaving the circle; starts over after that
    bool tick(const Pos& p, bool grounded, bool trying, uint64_t dt) {
      if(!grounded) {
        // Falling into a hole must not count, and the circle starts where he lands
        hasAnchor = false;
        return false;
        }
      if(!hasAnchor || std::hypot(p.x-anchor.x,p.z-anchor.z)>Radius) {
        hasAnchor = true;
        anchor    = p;
        tried     = 0;
        return false;
        }
      if(trying)
        tried += dt;
      if(tried<TryMs)
        return false;
      tried = 0;
      return true;
      }

    Pos stuckAt() const { return anchor; }

  private:
    Pos      anchor = {};
    bool     hasAnchor = false;
    uint64_t tried = 0;
  };
