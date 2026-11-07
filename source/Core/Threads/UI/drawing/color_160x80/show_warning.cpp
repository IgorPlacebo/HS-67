#include "Buttons.hpp"
#include "OperatingModeUtilities.h"
#include "OperatingModes.h"
#include "Translation.h"
#include "ui_drawing.hpp"
#ifdef LCD_160x80
bool warnUser(const char *warning, const ButtonState buttons) {
  // Startup / safety messages use the colour alert layout. The GUI thread refreshes the frame
  // afterwards.
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
  // HS-02 has no mono screens any more: anything unexpected uses the generic fault layout.
  Display::setColorMode(true);
  Display::clearScreenColor();
  ui_draw_warning(which >= 0 ? (uint8_t)which : (uint8_t)HS02_WARN_RUNAWAY);
  // Also timeout after 5 seconds
  if ((xTaskGetTickCount() - lastButtonTime) > TICKS_SECOND * 5) {
    return true;
  }
  return buttons != BUTTON_NONE;
}
#endif
