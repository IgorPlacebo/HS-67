// HS-02 three-point tip calibration (Settings > Tip > Calibration), like the stock firmware:
// the iron heats to 150, 250 and 350 C (by its current reading); at each point, once stable,
// the user enters what an external tip thermometer shows (+/-) and confirms with OK.
//
// From the three (true temperature, raw ADC count) pairs we derive the raw counts at the stock
// anchor temperatures 140/240/340 C and store them in the stock calibration words, so the result
// is also valid for the stock firmware. Hold OK to abort; the heater is switched off on exit.
#include "BSP.h"
#include "Beeper.h"
#include "Hs02Controls.h"
#include "OperatingModes.h"
#include "TipThermoModel.h"
#include "configuration.h"
#include "ui_drawing.hpp"
#include "../drawing/color_160x80/Hs02UiStrings.hpp"

#ifdef LCD_160x80

bool     hs02WriteTipCal(uint32_t c140, uint32_t c240, uint32_t c340); // ThermoModel.cpp
uint32_t hs02TipCountX10FromuV(uint32_t tipuV);                        // ThermoModel.cpp
extern uint8_t heaterThermalRunawayCounter;

namespace {
constexpr TemperatureType_t kSetpoints[3] = {150, 250, 350};
constexpr TickType_t        kSettleTime   = 3 * TICKS_SECOND; // within +/-3 C for this long = ready
constexpr uint8_t           kDone = 3, kFailed = 4;

int32_t truthX10[3]; // what the model must output at each point (x10 C, see below)
int32_t countX10[3]; // raw stock ADC count x10 measured at each point

// Raw count (x10) at temperature tX10 on the piecewise line through the three measured points.
int32_t countAt(int32_t tX10) {
  const uint8_t i = (tX10 <= truthX10[1]) ? 0 : 1; // segment 0-1 (also extrapolates down), else 1-2
  return countX10[i] + (tX10 - truthX10[i]) * (countX10[i + 1] - countX10[i]) / (truthX10[i + 1] - truthX10[i]);
}

bool finish() {
  for (uint8_t i = 0; i < 2; i++) {
    if (truthX10[i + 1] <= truthX10[i] || countX10[i + 1] <= countX10[i]) {
      return false;
    }
  }
  return hs02WriteTipCal((uint32_t)((countAt(1400) + 5) / 10), (uint32_t)((countAt(2400) + 5) / 10), (uint32_t)((countAt(3400) + 5) / 10));
}
} // namespace

// scratch_state: state1 = step (0..2, 3 done, 4 failed), state2 = entered reading (C),
//                state3 = tick since which the tip is stable (0 = not stable)
OperatingMode gui_tipCalibration(const ButtonState buttons, guiContext *cxt) {
  auto &st            = cxt->scratch_state;
  cxt->transitionMode = TransitionAnimation::None;

  if (st.state1 >= kDone) {
    currentTempTargetDegC = 0;
    const bool ok         = st.state1 == kDone;
    ui_draw_alert(Hs02UiText::kCalTitle(), ok ? Hs02UiText::kCalDone1() : Hs02UiText::kCalFail1(), ok ? Hs02UiText::kCalDone2() : Hs02UiText::kCalFail2(),
                  Hs02UiText::kExitHint(), ok ? 0xFF4DB8 : 0xFF3B30); // pink = done, red = failed (draw_home_gauge.cpp palette)
    return (buttons == BUTTON_OK_SHORT || buttons == BUTTON_OK_LONG) ? OperatingMode::ModernMenu : OperatingMode::TipCalibration;
  }

  // Abort conditions: user, missing/shorted tip, thermal runaway.
  if (buttons == BUTTON_OK_LONG || isTipDisconnected() || isTipShorted()) {
    currentTempTargetDegC = 0;
    beep(40);
    return OperatingMode::ModernMenu;
  }
  if (heaterThermalRunawayCounter > 8) {
    currentTempTargetDegC       = 0;
    heaterThermalRunawayCounter = 0;
    return OperatingMode::ThermalRunaway;
  }

  const uint8_t           step     = (uint8_t)st.state1;
  const TemperatureType_t setpoint = kSetpoints[step];
  if (st.state2 == 0) {
    st.state2 = setpoint; // start the entry at the nominal value
  }
  currentTempTargetDegC = setpoint; // the iron's own idea of temperature (current calibration)
  lastMovementTime = lastButtonTime = xTaskGetTickCount(); // keep sleep/shutdown logic quiet

  const TickType_t        now     = xTaskGetTickCount();
  const TemperatureType_t current = TipThermoModel::getTipInC();
  const int32_t           error   = (int32_t)setpoint - (int32_t)current;
  if (error >= -3 && error <= 3) {
    if (st.state3 == 0) {
      st.state3 = now;
    }
  } else {
    st.state3 = 0;
  }
  const bool ready = st.state3 != 0 && (now - st.state3) >= kSettleTime;

  // +/- adjust the thermometer reading (held: accelerating repeat).
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
    break;
  }
  if (direction) {
    const uint8_t m = hs02RepeatMultiplier(held);
    if (m) {
      if (getSettingValue(SettingsOptions::ReverseButtonTempChangeEnabled)) {
        direction = -direction;
      }
      int32_t v = (int32_t)st.state2 + direction * m;
      if (v < setpoint - 120) {
        v = setpoint - 120;
      } else if (v > setpoint + 120) {
        v = setpoint + 120;
      }
      st.state2 = (uint16_t)v;
    }
  }

  if (buttons == BUTTON_OK_SHORT) {
    if (!ready) {
      beep(20); // not stable yet
    } else {
      // The model (with stock-format calibration) outputs curve(count) - 25 C + handle temperature,
      // so for the display to equal the thermometer the curve must give: truth + 25 C - handle.
      const uint32_t tipuV   = TipThermoModel::convertTipRawADCTouV(getTipRawTemp(0));
      countX10[step]         = (int32_t)hs02TipCountX10FromuV(tipuV);
      truthX10[step]         = (int32_t)st.state2 * 10 + 250 - (int32_t)getHandleTemperature(0);
      beep(80);
      st.state1 = step + 1;
      st.state2 = 0;
      st.state3 = 0;
      if (st.state1 == kDone) {
        currentTempTargetDegC = 0;
        st.state1             = finish() ? kDone : kFailed;
      }
      return OperatingMode::TipCalibration;
    }
  }

  ui_draw_calibration(step, current, setpoint, st.state2, ready);
  return OperatingMode::TipCalibration;
}

#else
OperatingMode gui_tipCalibration(const ButtonState buttons, guiContext *cxt) {
  (void)buttons;
  (void)cxt;
  return OperatingMode::HomeScreen;
}
#endif
