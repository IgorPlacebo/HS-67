#ifndef HS02_CONTROLS_H_
#define HS02_CONTROLS_H_
// FNIRSI-style controls shared by the HS-02 standby and soldering screens:
//   +/- short  : change the target by one step (Settings > Tip > Step, default 5)
//   +/- hold   : keep changing (~8 per second), accelerating: x2 after 1 s, x5 after 2.5 s
//   OK short   : (soldering) jump to the next preset (Settings > Tip > Preset 1/2/3)
// Target changes are written to flash once the buttons have been idle for 2 s.
#include "Buttons.hpp"
#include <stdint.h>

bool    hs02StepTarget(ButtonState buttons); // true if the button was a +/- action (handled)
void    hs02NextPreset();                    // Target -> next preset above it (wraps to preset 1)
uint8_t hs02PresetIndex(uint16_t target);    // 1..3 if target equals a preset, else 0
void    hs02ControlsService();               // Call every frame: deferred settings save
void    hs02ControlsFlush();                 // Save a pending change right now (mode changes)
// Repeat helper for held +/- buttons: returns 0 while rate-limited, else the step multiplier
// (1, 2 after 1 s of holding, 5 after 2.5 s). Short presses always return 1.
uint8_t hs02RepeatMultiplier(bool held);

#endif
