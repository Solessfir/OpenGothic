#include "../../common/utils/gamepadactivity.h"

#include <iostream>
#include <stdexcept>

int main() {
  try {
    GamepadActivity activity;
    auto sample=[&](bool connected,uint32_t buttons=0,float x=0.f,float trigger=0.f) {
      return activity.update(connected,buttons,x,0,0,0,trigger,0,0.2f,0.55f,0.4f);
      };
    auto check=[](bool ok,const char* text) { if(!ok) throw std::runtime_error(text); };
    check(!sample(true),"Connecting an idle controller leaves touch active");
    check(!sample(true,0,0.1f),"Stick drift cannot take over touch");
    check(sample(true,0,0.5f),"A fresh stick tilt takes over");
    check(!sample(true,0,0.8f),"A held stick cannot steal control back from touch");
    check(!sample(true,0,0.18f),"Hysteresis ignores jitter near the activation boundary");
    check(!sample(true),"Centering a stick is not a takeover");
    check(sample(true,0,0.5f),"Centering and tilting again takes over");
    check(sample(true,1,0.5f),"A new button can take over while a stick is held");
    check(!sample(true,1,0.5f),"A held button cannot steal control");
    check(!sample(true),"Releasing a button leaves touch active");
    check(sample(true,0,0,0.8f),"A trigger press takes over");
    check(!sample(true,0,0,0.9f),"Holding a trigger does not take over again");
    check(!sample(false),"Disconnecting leaves touch available");
    check(!sample(true,1,0.8f,0.9f),"Reconnect with held controls requires releasing first");
    check(!sample(true),"Reconnect release is not a press");
    check(sample(true,1),"A fresh press after reconnect works");
    std::cout << "Input takeover checks passed\n";
    }
  catch(const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
    }
  }
