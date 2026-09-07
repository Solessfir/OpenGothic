#include "../../common/utils/touchadjustment.h"

#include <iostream>
#include <stdexcept>

void check(bool ok, const char* message) {
  if(!ok) throw std::runtime_error(message);
  }

int main() {
  try {
    TouchAdjustment slider;
    check(slider.drag(0,20)==0,"Stationary input does not change the value");
    check(slider.drag(8,20)==0 && slider.drag(11,20)==0,"Small drag samples accumulate without losing precision");
    check(slider.drag(1,20)==1,"Crossing one step increases the value once");
    check(slider.drag(0,20)==0,"Holding position does not repeatedly increase volume");
    check(slider.drag(-60,20)==-3,"A longer left drag decreases the value proportionally");
    check(slider.drag(100,20)==5,"Batched motion preserves every slider step");
    check(slider.drag(12,20)==0 && slider.drag(-12,20)==0,"Small direction reversals cancel cleanly");
    slider.drag(19,20);
    slider.reset();
    check(slider.drag(1,20)==0,"Lifting the finger or changing context clears residual movement");
    slider.reset();
    check(slider.drag(1,0)==1,"A zero step cannot divide by zero");
    std::cout << "Touch adjustment tests passed\n";
    }
  catch(const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
    }
  }
