/*
 * LCD_Port.cpp
 *
 *  Created on: 27 May 2026
 *      Author: OK2CM
 */

#pragma once
#include "n32l40x_tim.h"
#undef LCD

// Set backlight brightness
// NOTE: configuration.h MAX_BRIGHTNESS is 105; values above 100 saturate to
// full-scale PWM (127) — intentional headroom, not a mismatch with MIN 1.
static inline void LCDSetBacklight(const uint8_t brightness){
  TIM_SetCmp3(TIM1, brightness > 100 ? 127 : brightness);
}
