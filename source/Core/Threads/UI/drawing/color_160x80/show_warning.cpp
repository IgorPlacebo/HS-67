#include "Buttons.hpp"
#include "OperatingModeUtilities.h"
#include "OperatingModes.h"
#include "Translation.h"
#include "ui_drawing.hpp"
#ifdef LCD_160x80
bool warnUser(const char *warning, const ButtonState buttons) {
  // Known startup / safety messages get the colour alert layout; anything else (rare legacy
  // messages) falls back to the mono full-screen text. Either way the GUI thread refreshes the
  // frame afterwards in whichever colour mode we leave selected.
  int8_t which = -1;
  if (warning == translatedString(Tr->SettingsResetMessage)) {
    which = HS02_WARN_SETTINGS_RESET;
  } else if (warning == translatedString(Tr->NoAccelerometerMessage)) {
    which = HS02_WARN_NO_ACCEL;
  } else if (warning == translatedString(Tr->NoPowerDeliveryMessage)) {
    which = HS02_WARN_NO_PD;
  } else if (warning == translatedString(Tr->WarningThermalRunaway)) {
    which = HS02_WARN_RUNAWAY;
  }
  if (which >= 0) {
    Display::setColorMode(true);
    Display::clearScreenColor();
    ui_draw_warning((uint8_t)which);
  } else {
    Display::setColorMode(false);
    Display::clearScreen();
    Display::printWholeScreen(warning);
  }
  // Also timeout after 5 seconds
  if ((xTaskGetTickCount() - lastButtonTime) > TICKS_SECOND * 5) {
    return true;
  }
  return buttons != BUTTON_NONE;
}
#endif
