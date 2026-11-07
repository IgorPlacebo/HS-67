#include "BSP.h"

// Boards without a melody player.
__attribute__((weak)) void playStartupMelody(void) {}
__attribute__((weak)) void stopStartupMelody(void) {}
