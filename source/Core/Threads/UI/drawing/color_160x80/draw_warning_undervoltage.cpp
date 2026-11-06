#include "ui_drawing.hpp"
#ifdef LCD_160x80

void ui_draw_warning_undervoltage_idle(void) {
  // Switch to mono mode for warning screen
  Display::setColorMode(false);
  Display::clearScreen();

  // Line 1 (y=16): "Низька"
  Display::setCursor(0, 16);
  Display::print(translatedString(Tr->UndervoltageString), FontStyle::SMALL);

  // Line 2 (y=44): "напруга 5.0V"
  Display::setCursor(0, 44);
  Display::print(translatedString(Tr->InputVoltageString), FontStyle::SMALL);
  printVoltage();
  Display::print(SmallSymbolVolts, FontStyle::SMALL);

  Display::refresh();
}

void ui_draw_warning_undervoltage(void) {
  ui_draw_warning_undervoltage_idle();
  GUIDelay();
  waitForButtonPress();
}
#endif
