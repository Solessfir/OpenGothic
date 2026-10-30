#include "mouseutil.h"

#include <Tempest/Platform>
#include <algorithm>

#include "gothic.h"

#ifdef __WINDOWS__
#include <windows.h>
#include <shlwapi.h>
#else
#include <sys/stat.h>
#endif

float MouseUtil::mouseSysSpeed() {
#ifdef __WINDOWS__
  int mouseSpeed = 10;
  if(SystemParametersInfoA(SPI_GETMOUSESPEED, 0, &mouseSpeed, 0)) {
    // MSDN says in between 1..20, but clamp to be extra safe
    mouseSpeed = std::clamp(mouseSpeed, 1, 20);
    return float(mouseSpeed)/10.f;
    }
  return 1.f;
#else
  return 1.f;
#endif
  }

static bool gamepadActive = false;

bool MouseUtil::isPointerEnabled() {
#if defined(__MOBILE_PLATFORM__)
  // Touch input does not use the pointer
  return false;
#else
  return !gamepadActive && Gothic::settingsGetI("GAME","enableMouse")!=0;
#endif
  }

bool MouseUtil::cursorPosition(int& x, int& y) {
#ifdef __WINDOWS__
  POINT p = {};
  if(!GetCursorPos(&p))
    return false;
  x = p.x;
  y = p.y;
  return true;
#else
  (void)x;
  (void)y;
  return false;
#endif
  }

void MouseUtil::setGamepadActive(bool active) {
  gamepadActive = active;
  }
