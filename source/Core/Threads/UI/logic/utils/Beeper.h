#ifndef UI_BEEPER_H_
#define UI_BEEPER_H_
#include <stdint.h>

// Non-blocking beeps for the UI. beep() starts the buzzer (at the volume chosen in
// Settings > System > Sound) and beeperService(), called every GUI frame, stops it.
void beep(uint16_t durationMs);
void beeperService();

#endif
