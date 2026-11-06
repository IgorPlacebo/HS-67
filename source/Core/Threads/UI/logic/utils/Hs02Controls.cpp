#include "Hs02Controls.h"
#include "BSP.h"
#include "Beeper.h"
#include "FreeRTOS.h"
#include "Settings.h"
#include "configuration.h"
#include "task.h"

static TickType_t lastRepeat  = 0;
static TickType_t pendingSave = 0; // 0 = nothing to save

static void markDirty() {
  pendingSave = xTaskGetTickCount();
  if (pendingSave == 0) {
    pendingSave = 1;
  }
}

static void setTarget(int32_t temp) {
  const bool    inF = getSettingValue(SettingsOptions::TemperatureInF);
  const int32_t lo  = inF ? MIN_TEMP_F : MIN_TEMP_C;
  const int32_t hi  = inF ? MAX_TEMP_F : MAX_TEMP_C;
  if (temp < lo) {
    temp = lo;
  } else if (temp > hi) {
    temp = hi;
  }
  if ((uint16_t)temp != getSettingValue(SettingsOptions::SolderingTemp)) {
    setSettingValue(SettingsOptions::SolderingTemp, (uint16_t)temp);
    markDirty();
  }
}

uint8_t hs02RepeatMultiplier(bool held) {
  static TickType_t holdStart = 0;
  const TickType_t  now       = xTaskGetTickCount();
  if (!held) {
    return 1;
  }
  if (now - lastRepeat > TICKS_SECOND / 3) {
    holdStart = now; // a new hold (repeats arrive every ~125 ms while held)
  } else if (now - lastRepeat < TICKS_SECOND / 8) {
    return 0; // rate-limit to ~8 steps per second
  }
  lastRepeat = now;
  const TickType_t heldFor = now - holdStart;
  return heldFor > (TICKS_SECOND * 5) / 2 ? 5 : (heldFor > TICKS_SECOND ? 2 : 1);
}

bool hs02StepTarget(ButtonState buttons) {
  int8_t direction = 0;
  bool   held      = false;
  switch (buttons) {
  case BUTTON_F_SHORT:
    direction = 1;
    break;
  case BUTTON_B_SHORT:
    direction = -1;
    break;
  case BUTTON_F_LONG:
    direction = 1;
    held      = true;
    break;
  case BUTTON_B_LONG:
    direction = -1;
    held      = true;
    break;
  default:
    return false;
  }
  const uint8_t multiplier = hs02RepeatMultiplier(held);
  if (!multiplier) {
    return true;
  }
  if (getSettingValue(SettingsOptions::ReverseButtonTempChangeEnabled)) {
    direction = -direction;
  }
  int32_t step = getSettingValue(SettingsOptions::TempChangeShortStep);
  if (step < 1) {
    step = 1;
  }
  int32_t temp = getSettingValue(SettingsOptions::SolderingTemp) + direction * step * multiplier;
  temp         = (temp / step) * step; // snap to the step grid like the stock UI
  setTarget(temp);
  return true;
}

uint8_t hs02PresetIndex(uint16_t target) {
  if (target == getSettingValue(SettingsOptions::PresetTemp1)) {
    return 1;
  }
  if (target == getSettingValue(SettingsOptions::PresetTemp2)) {
    return 2;
  }
  if (target == getSettingValue(SettingsOptions::PresetTemp3)) {
    return 3;
  }
  return 0;
}

void hs02NextPreset() {
  const uint16_t presets[3] = {getSettingValue(SettingsOptions::PresetTemp1), getSettingValue(SettingsOptions::PresetTemp2),
                               getSettingValue(SettingsOptions::PresetTemp3)};
  const uint8_t  current    = hs02PresetIndex(getSettingValue(SettingsOptions::SolderingTemp));
  // From a preset go to the next one; from a custom value start at preset 1.
  const uint8_t next = current ? (current % 3) : 0;
  setTarget(presets[next]);
  beep(40);
}

void hs02ControlsFlush() {
  if (pendingSave) {
    pendingSave = 0;
    saveSettings();
  }
}

void hs02ControlsService() {
  if (pendingSave && (xTaskGetTickCount() - pendingSave) > 2 * TICKS_SECOND) {
    pendingSave = 0;
    saveSettings();
  }
}
