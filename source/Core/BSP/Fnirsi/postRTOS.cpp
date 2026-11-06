#include "BSP.h"

// Initialisation to be performed with scheduler active
void postRToSInit() {
  // Settings are loaded by now: request the user's chosen USB-PD voltage (default 20V,
  // the same level Setup.cpp configures at power-on).
  applyPDVoltageSetting();
}
