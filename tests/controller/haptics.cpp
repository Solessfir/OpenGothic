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
    std::cout << "Haptic policy tests passed\n";
    }
  catch(const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
    }
  }
