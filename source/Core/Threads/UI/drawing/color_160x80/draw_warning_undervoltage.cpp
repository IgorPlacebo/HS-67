#include "ui_drawing.hpp"
#ifdef LCD_160x80
// The colour ui_draw_warning_undervoltage_idle() lives in draw_home_gauge.cpp.
void ui_draw_warning_undervoltage(void) {
  ui_draw_warning_undervoltage_idle();
  Display::refresh();
  GUIDelay();
  waitForButtonPress();
}
#endif
