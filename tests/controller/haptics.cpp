#include "../../common/utils/hapticpolicy.h"

#include <iostream>
#include <stdexcept>

void check(bool ok, const char* message) {
  if(!ok) throw std::runtime_error(message);
  }

int main() {
  try {
    using Effect=Feedback::Effect;
    HapticPolicy h;
    check(h.next(Effect::Damage,0).duration==0,"Feedback stays silent until enabled");
    check(h.setEnabled(true),"Enabling requests cleanup of any previous pulse");
    const auto navigation=h.next(Effect::Navigate,1);
    check(navigation.duration==8 && !navigation.gamepad,"Touch navigation uses a short phone pulse");
    check(h.next(Effect::Navigate,2).duration==0,"Repeated navigation is rate limited");
    const auto confirmation=h.next(Effect::Confirm,3);
    check(confirmation.strength>navigation.strength,"Confirmation overrides lighter navigation");
    const auto hit=h.next(Effect::Hit,4);
    const auto damage=h.next(Effect::Damage,5);
    check(damage.strength>hit.strength && hit.strength>confirmation.strength,"Damage and landed hits have distinct strengths");
    check(h.next(Effect::Navigate,100).duration==0,"Navigation cannot interrupt a damage pulse");
    check(h.next(Effect::Damage,100).duration==0,"Rapid damage events cannot cause continuous rumble");
    check(h.next(Effect::Navigate,185).duration!=0,"Feedback resumes after the cooldown");
    check(h.setGamepad(true),"Changing devices requests cancellation");
    check(h.next(Effect::Navigate,186).gamepad,"Controller feedback never falls back to the phone");
    check(!h.setGamepad(true),"Polling the same device does not reset the cooldown");
    check(h.setEnabled(false),"Disabling requests immediate cancellation");
    check(h.next(Effect::Damage,1000).duration==0,"Off suppresses every effect");
    h.setEnabled(true);
    check(h.next(Effect::Confirm,1001).duration!=0,"Re-enabling allows confirmation immediately");
    check(h.charge(0.f,1002).duration==0,"Charging cannot interrupt confirmation");
    const auto low=h.charge(0.f,1200);
    check(low.gamepad && low.duration>0,"Charging respects the selected feedback device");
    check(h.charge(1.f,1201).duration==0,"Charging is limited to four pulses per second");
    const auto high=h.charge(1.f,1450);
    check(high.strength>low.strength && high.duration>low.duration,"Teleport charging grows stronger");
    const auto teleport=h.next(Effect::Teleport,1451);
    check(teleport.strength>high.strength,"Actual teleport overrides charging with a stronger pulse");
    check(h.charge(1.f,1452).duration==0,"Charging cannot interrupt teleport feedback");
    const auto shot=h.next(Effect::Shoot,1700);
    check(shot.duration>0 && shot.strength<teleport.strength,"Arrow release has its own lighter pulse");
    check(h.next(Effect::Hit,1701).duration>0,"A landed hit takes priority over firing feedback");
    check(h.next(Effect::Cast,1900).duration>0,"Spell emission has feedback");
    h.setEnabled(false);
    check(h.charge(1.f,3000).duration==0 && h.next(Effect::Teleport,3000).duration==0,"Off suppresses charging and teleportation");
    std::cout << "Haptic policy tests passed\n";
    }
  catch(const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
    }
  }
