// Modern colour home / soldering / sleep screens for the HS-02 family (160x80 ST7735).
//
// Visual language borrowed from the "Agent Status Bar" project: pure black background,
// a status pill (tinted capsule + dot + label) that carries the state colour, a big white
// readout, grey secondary text and a segmented progress bar.
//
//   y  0..17  status row   [● Нагрев]                         ϟ 20.0 В
//   y 18..49  value row    318°                               цель 320°
//   y 50..57  progress     ▬▬▬▬▬ ▬▬▬▬▬ ▬▬▬░░ ░░░░░ ░░░░░   (tip temp / target)
//   y 58..79  footer       42.5 Вт                       сон через 4м
//
// The framebuffer is 2bpp, so every row band gets its own 4-colour palette
// (Display::setColorBands) -- that is how a black / white / grey / accent / tinted-accent
// screen fits into 2 bits per pixel. Text is Inter (Hs02ModernFont.hpp), with a dim
// "edge" colour giving two-level anti-aliasing where the band palette has room for it.
//
// State colours (iOS system palette, as on the status bar):
//   standby grey · heating orange · at temperature green · boost red · sleep cyan · tip fault pink
#include "Hs02ModernFont.hpp"
#include "Hs02UiStrings.hpp"
#include "OperatingModes.h"
#include "TipThermoModel.h"
#include "power.hpp"
#include "ui_drawing.hpp"
#include "../../../../../version.h" // source/version.h (BUILD_VERSION)

#ifdef LCD_160x80

namespace {

// ---------------------------------------------------------------- colours & bands
constexpr uint16_t rgb565(uint32_t rgb) { return (uint16_t)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F)); }

constexpr uint32_t kBlack     = 0x000000;
constexpr uint32_t kWhite     = 0xFFFFFF;
constexpr uint32_t kGray      = 0x8E8E93; // secondary text
constexpr uint32_t kEdge      = 0x3A3A3C; // anti-alias edge for white/grey text
constexpr uint32_t kTrack     = 0x2C2C2E; // empty progress segment
constexpr uint32_t kStandby   = 0x8E8E93;
constexpr uint32_t kHeatingC  = 0xFF9F0A;
constexpr uint32_t kReadyC    = 0x30D158;
constexpr uint32_t kBoostC    = 0xFF453A;
constexpr uint32_t kSleepC    = 0x64D2FF;
constexpr uint32_t kFaultC    = 0xFF2D55;
constexpr uint32_t kCoolingC  = 0x40C8E0; // teal: target lowered, tip cooling down
constexpr uint8_t  kHotTempC  = 50;       // standby warns while the tip is hotter than this

// Row bands. Colour index meaning per band:
//   status : 0 bg, 1 accent, 2 pill tint, 3 grey
//   value  : 0 bg, 1 white,  2 edge,      3 grey
//   bar    : 0 bg, 1 accent, 2 tint,      3 track
//   footer : 0 bg, 1 white,  2 edge,      3 grey
constexpr uint8_t kBandStatusY = 0, kBandValueY = 18, kBandBarY = 50, kBandFooterY = 58;
constexpr uint8_t kBg = 0;
constexpr uint8_t kAccent = 1, kTint = 2, kTrackIdx = 3; // status / bar bands
constexpr uint8_t kInk = 1, kEdgeIdx = 2, kGrayIdx = 3;  // value / footer bands (grey also in status)
constexpr uint8_t kNoEdge = 0xFF;

ColorBand bands[4];

uint32_t mixRgb(uint32_t a, uint32_t b, uint8_t amountOfA /* 0..255 */) {
  uint32_t out = 0;
  for (uint8_t shift = 0; shift <= 16; shift += 8) {
    const uint32_t ca = (a >> shift) & 0xFF, cb = (b >> shift) & 0xFF;
    out |= ((ca * amountOfA + cb * (255 - amountOfA)) / 255) << shift;
  }
  return out;
}

void applyTheme(uint32_t accent) {
  const uint16_t black = rgb565(kBlack), white = rgb565(kWhite), gray = rgb565(kGray), edge = rgb565(kEdge);
  const uint16_t acc = rgb565(accent), tint = rgb565(mixRgb(accent, kBlack, 60));
  bands[0] = {kBandStatusY, {black, acc, tint, gray}};
  bands[1] = {kBandValueY, {black, white, edge, gray}};
  bands[2] = {kBandBarY, {black, acc, tint, rgb565(kTrack)}};
  bands[3] = {kBandFooterY, {black, white, edge, gray}};
  Display::setColorBands(bands, 4);
}

// ---------------------------------------------------------------- text
using Hs02ModernFont::Face;
using Hs02ModernFont::Glyph;

// Decodes one UTF-8 code point (the UI strings only use 1- and 2-byte sequences, plus "—").
uint16_t nextCodePoint(const char *&s) {
  const uint8_t c = (uint8_t)*s++;
  if (c < 0x80) {
    return c;
  }
  if ((c & 0xE0) == 0xC0) {
    const uint8_t c1 = (uint8_t)*s;
    if (c1) {
      s++;
    }
    return (uint16_t)(((c & 0x1F) << 6) | (c1 & 0x3F));
  }
  if ((c & 0xF0) == 0xE0) {
    uint16_t cp = c & 0x0F;
    for (uint8_t i = 0; i < 2 && *s; i++) {
      cp = (uint16_t)((cp << 6) | ((uint8_t)*s++ & 0x3F));
    }
    return cp;
  }
  return '?';
}

const Glyph *findGlyph(const Face &face, uint16_t code) {
  uint8_t lo = 0, hi = face.count;
  while (lo < hi) {
    const uint8_t mid = (lo + hi) / 2;
    if (face.glyphs[mid].code < code) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  return (lo < face.count && face.glyphs[lo].code == code) ? &face.glyphs[lo] : nullptr;
}

uint8_t textWidth(const Face &face, const char *text) {
  uint8_t width = 0;
  while (*text) {
    const Glyph *g = findGlyph(face, nextCodePoint(text));
    if (g) {
      width += g->advance;
    }
  }
  return width;
}

// Draws `text` with its top-left at (x, y); returns the pen position after the last glyph.
// Solid pixels use `color`; anti-alias edge pixels use `edge` (or are dropped with kNoEdge).
uint8_t drawText(const Face &face, const char *text, uint8_t x, uint8_t y, uint8_t color, uint8_t edge = kNoEdge) {
  while (*text) {
    const Glyph *g = findGlyph(face, nextCodePoint(text));
    if (!g) {
      continue;
    }
    const uint8_t  rowBytes = (g->width + 3) / 4;
    const uint8_t *data     = face.data + g->offset;
    for (uint8_t gy = 0; gy < face.height; gy++) {
      for (uint8_t gx = 0; gx < g->width; gx++) {
        const uint8_t v = (data[gy * rowBytes + gx / 4] >> ((gx % 4) * 2)) & 0x3;
        if (v == 3) {
          Display::fillRectColor(x + g->xOffset + gx, y + gy, 1, 1, color);
        } else if (v && edge != kNoEdge) {
          Display::fillRectColor(x + g->xOffset + gx, y + gy, 1, 1, edge);
        }
      }
    }
    x += g->advance;
  }
  return x;
}

// Top of a face's bitmap such that its baseline lands on `baselineY`.
uint8_t topFor(const Face &face, uint8_t baselineY) { return baselineY - face.baseline; }

// Small unsigned / tenths formatters into a caller buffer (no printf on this MCU build).
char *formatUnsigned(char *out, uint32_t value) {
  char    tmp[10];
  uint8_t n = 0;
  do {
    tmp[n++] = (char)('0' + value % 10);
    value /= 10;
  } while (value && n < sizeof(tmp));
  while (n) {
    *out++ = tmp[--n];
  }
  *out = '\0';
  return out;
}

char *formatTenths(char *out, uint32_t tenths) {
  out    = formatUnsigned(out, tenths / 10);
  *out++ = '.';
  *out++ = (char)('0' + tenths % 10);
  *out   = '\0';
  return out;
}

char *append(char *out, const char *s) {
  while (*s) {
    *out++ = *s++;
  }
  *out = '\0';
  return out;
}

// ---------------------------------------------------------------- shapes
// Capsule / rounded rectangle with radius r (r <= h/2), filled with `color`.
void fillRounded(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r, uint8_t color) {
  for (uint8_t row = 0; row < h; row++) {
    uint8_t inset = 0;
    const int8_t dyTop = (int8_t)r - 1 - (int8_t)row, dyBot = (int8_t)row - (int8_t)(h - r);
    const int8_t dy    = dyTop > 0 ? dyTop : (dyBot > 0 ? dyBot : -1);
    if (dy >= 0) {
      // Inset so that the corner follows a circle of radius r (sampled at pixel centres).
      const int16_t rr = (int16_t)r * r * 4;
      while (inset < r) {
        const int16_t dx = (int16_t)(r - inset) * 2 - 1, dyy = (int16_t)dy * 2 + 1;
        if (dx * dx + dyy * dyy <= rr) {
          break;
        }
        inset++;
      }
    }
    if (w > 2 * inset) {
      Display::fillRectColor(x + inset, y + row, w - 2 * inset, 1, color);
    }
  }
}

void fillDot(uint8_t cx, uint8_t cy, uint8_t color) {
  // 5x5 disc
  Display::fillRectColor(cx - 1, cy - 2, 3, 5, color);
  Display::fillRectColor(cx - 2, cy - 1, 5, 3, color);
}

// 5x8 lightning bolt, rows top to bottom, bit 4 = leftmost pixel.
constexpr uint8_t kBolt[8] = {0b00011, 0b00110, 0b01100, 0b11111, 0b00110, 0b01100, 0b11000, 0b10000};
void drawBolt(uint8_t x, uint8_t y, uint8_t color) {
  for (uint8_t row = 0; row < 8; row++) {
    for (uint8_t col = 0; col < 5; col++) {
      if (kBolt[row] & (0x10 >> col)) {
        Display::fillRectColor(x + col, y + row, 1, 1, color);
      }
    }
  }
}

// ---------------------------------------------------------------- building blocks
constexpr uint8_t kScreenW = 160, kMargin = 4;

void drawStatusPill(const char *label) {
  constexpr uint8_t x = kMargin, y = 3, h = 13;
  const uint8_t     w = 15 + textWidth(Hs02ModernFont::kLabel, label) + 7;
  fillRounded(x, y, w, h, h / 2, kTint);
  fillDot(x + 8, y + 6, kAccent);
  drawText(Hs02ModernFont::kLabel, label, x + 15, topFor(Hs02ModernFont::kLabel, y + 10), kAccent);
}

void drawSupplyVoltage() {
  char           text[12];
  const uint32_t volts = getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);
  char          *p     = formatTenths(text, volts);
  *p++                 = ' ';
  append(p, Hs02UiText::kVolt());
  const uint8_t width = textWidth(Hs02ModernFont::kLabel, text);
  const uint8_t x     = kScreenW - kMargin - width;
  drawText(Hs02ModernFont::kLabel, text, x, topFor(Hs02ModernFont::kLabel, 13), kGrayIdx);
  drawBolt(x - 8, 5, kGrayIdx);
}

constexpr uint8_t kValueBaseline = 46;

// Big temperature with a grey degree sign; `dashes` shows "---" (no reading).
void drawHeroTemperature(TemperatureType_t temp, bool dashes = false) {
  char text[6];
  if (dashes) {
    append(text, "---");
  } else {
    formatUnsigned(text, temp > 999 ? 999 : temp);
  }
  const uint8_t end = drawText(Hs02ModernFont::kHero, text, kMargin, topFor(Hs02ModernFont::kHero, kValueBaseline), dashes ? kGrayIdx : kInk, kEdgeIdx);
  if (!dashes) {
    drawText(Hs02ModernFont::kValue, "°", end + 1, 20, kGrayIdx, kEdgeIdx);
  }
}

// Right column of the value row: a grey caption over a white value, right-aligned.
void drawTarget(const char *caption, TemperatureType_t target) {
  char text[8];
  append(formatUnsigned(text, target), "°");
  const uint8_t valueW = textWidth(Hs02ModernFont::kValue, text);
  drawText(Hs02ModernFont::kValue, text, kScreenW - kMargin - valueW, topFor(Hs02ModernFont::kValue, kValueBaseline), kInk, kEdgeIdx);
  const uint8_t capW = textWidth(Hs02ModernFont::kLabel, caption);
  drawText(Hs02ModernFont::kLabel, caption, kScreenW - kMargin - capW, topFor(Hs02ModernFont::kLabel, 30), kGrayIdx, kEdgeIdx);
}

// Five rounded segments; `fillX1000` is the filled fraction in 0..1000. The segment the
// fill ends in shows its remainder tinted, untouched segments show the grey track.
// `allTint` paints every empty segment tinted (used for fault screens).
void drawProgress(uint16_t fillX1000, bool allTint = false) {
  constexpr uint8_t segments = 5, gap = 3, y = 51, h = 5;
  constexpr uint8_t segW = (kScreenW - 2 * kMargin - (segments - 1) * gap) / segments; // 28
  if (fillX1000 > 1000) {
    fillX1000 = 1000;
  }
  for (uint8_t i = 0; i < segments; i++) {
    const uint8_t  x        = kMargin + i * (segW + gap);
    const uint16_t segStart = (uint16_t)i * 1000 / segments, segEnd = (uint16_t)(i + 1) * 1000 / segments;
    const bool     current  = fillX1000 > segStart && fillX1000 < segEnd;
    const uint8_t  emptyIdx = (allTint || current) ? kTint : kTrackIdx;
    fillRounded(x, y, segW, h, 2, emptyIdx);
    if (fillX1000 > segStart) {
      const uint16_t part   = (fillX1000 >= segEnd) ? segW : (uint16_t)(fillX1000 - segStart) * segW * segments / 1000;
      const uint8_t  filled = part < 2 ? 2 : (uint8_t)part;
      // Paint the filled part as a rounded bar of its own so its leading edge is rounded too.
      fillRounded(x, y, filled, h, 2, kAccent);
    }
  }
}

constexpr uint8_t kFooterBaseline = 75;

// Footer left: "42.5 Вт" style power readout.
void drawPower(uint32_t x10Watt) {
  char text[10];
  formatTenths(text, x10Watt);
  const uint8_t end = drawText(Hs02ModernFont::kValue, text, kMargin, topFor(Hs02ModernFont::kValue, kFooterBaseline), kInk, kEdgeIdx);
  drawText(Hs02ModernFont::kLabel, Hs02UiText::kWatt(), end + 3, topFor(Hs02ModernFont::kLabel, kFooterBaseline), kGrayIdx, kEdgeIdx);
}

void drawFooterHint(const char *hint) { drawText(Hs02ModernFont::kLabel, hint, kMargin, topFor(Hs02ModernFont::kLabel, kFooterBaseline), kGrayIdx, kEdgeIdx); }

// Footer right: "сон через 4м" until motion sleep kicks in.
void drawSleepCountdown() {
#ifndef NO_SLEEP_MODE
  if (!getSettingValue(SettingsOptions::Sensitivity) || !getSettingValue(SettingsOptions::SleepTime)) {
    return;
  }
  const TickType_t lastEventTime = lastButtonTime < lastMovementTime ? lastMovementTime : lastButtonTime;
  const TickType_t downCount     = getSleepTimeout() - xTaskGetTickCount() + lastEventTime;
  const bool       minutes       = downCount >= TICKS_MIN;
  const uint32_t   amount        = minutes ? (downCount + TICKS_MIN - 1) / TICKS_MIN : downCount / TICKS_SECOND + 1;
  char             text[32];
  char            *p = append(text, Hs02UiText::kSleepIn());
  *p++               = ' ';
  p                  = formatUnsigned(p, amount);
  append(p, minutes ? Hs02UiText::kMinutes() : Hs02UiText::kSeconds());
  const uint8_t width = textWidth(Hs02ModernFont::kLabel, text);
  drawText(Hs02ModernFont::kLabel, text, kScreenW - kMargin - width, topFor(Hs02ModernFont::kLabel, kFooterBaseline), kGrayIdx, kEdgeIdx);
#endif
}

uint16_t progressX1000(TemperatureType_t current, TemperatureType_t target) {
  if (target == 0) {
    return 0;
  }
  const uint32_t p = (uint32_t)current * 1000 / target;
  return p > 1000 ? 1000 : (uint16_t)p;
}

// Tip fault screens share one layout.
bool drawTipFault() {
  const bool open = isTipDisconnected(), shorted = !open && isTipShorted();
  if (!open && !shorted) {
    return false;
  }
  applyTheme(kFaultC);
  drawStatusPill(open ? Hs02UiText::kNoTip() : Hs02UiText::kShorted());
  drawSupplyVoltage();
  drawHeroTemperature(0, true);
  drawProgress(0, true);
  drawFooterHint(open ? Hs02UiText::kHintTip() : Hs02UiText::kHintShort());
  return true;
}

// "At temperature" with hysteresis so the pill doesn't flicker green/orange under load.
bool atTemperature(TemperatureType_t current, TemperatureType_t target) {
  static bool ready = false;
  const int   diff  = (int)target - (int)current;
  if (ready) {
    ready = diff < 15 && diff > -25;
  } else {
    ready = diff <= 5 && diff >= -10;
  }
  return ready;
}

// "цель" or, when the target equals one of the presets, "пресет 2".
const char *targetCaption(uint16_t target, char *buf) {
  uint8_t index = 0;
  if (target == getSettingValue(SettingsOptions::PresetTemp1)) {
    index = 1;
  } else if (target == getSettingValue(SettingsOptions::PresetTemp2)) {
    index = 2;
  } else if (target == getSettingValue(SettingsOptions::PresetTemp3)) {
    index = 3;
  }
  if (!index) {
    return Hs02UiText::kTarget();
  }
  char *p = append(buf, Hs02UiText::kPreset());
  *p++    = ' ';
  *p++    = (char)('0' + index);
  *p      = '\0';
  return buf;
}

} // namespace

void ui_draw_home_gauge_idle(TemperatureType_t tipTemp) {
  if (drawTipFault()) {
    return;
  }
  const TemperatureType_t target = getSettingValue(SettingsOptions::SolderingTemp);
  const bool              inF    = getSettingValue(SettingsOptions::TemperatureInF);
  const bool              hot    = tipTemp >= (inF ? 122 : kHotTempC);
  if (hot) {
    // Heater is off but the tip is still hot: blinking red "Hot!" pill (500 ms on / off).
    const bool on = ((xTaskGetTickCount() / (TICKS_SECOND / 2)) & 1) == 0;
    applyTheme(on ? kBoostC : kStandby);
    drawStatusPill(Hs02UiText::kHot());
  } else {
    applyTheme(kStandby);
    drawStatusPill(Hs02UiText::kStandby());
  }
  drawSupplyVoltage();
  drawHeroTemperature(tipTemp);
  char caption[24];
  drawTarget(targetCaption(target, caption), target);
  drawProgress(0);
  drawFooterHint(Hs02UiText::kHintHeat());
}

// Heater load as 0..1000 of the effective power limit (Settings > Tip > Max power).
static uint16_t powerLoadX1000(uint32_t x10Watt) {
  uint32_t limitW = getSettingValue(SettingsOptions::PowerLimit);
  if (limitW == 0 || limitW * 10 > HARDWARE_MAX_WATTAGE_X10) {
    limitW = HARDWARE_MAX_WATTAGE_X10 / 10;
  }
  const uint32_t load = x10Watt * 100 / limitW; // x10W * 1000 / (limitW * 10)
  return load > 1000 ? 1000 : (uint16_t)load;
}

void ui_draw_home_gauge_soldering(bool boostModeOn) {
  if (drawTipFault()) {
    return;
  }
  const TemperatureType_t current = TipThermoModel::getTipInC();
  const TemperatureType_t target  = getSettingValue(boostModeOn ? SettingsOptions::BoostTemp : SettingsOptions::SolderingTemp);
  const bool              ready   = atTemperature(current, target);
  const bool              cooling = !ready && current > target + 10;
  const uint32_t          x10Watt = x10WattHistory.average();
  if (boostModeOn) {
    applyTheme(kBoostC);
    drawStatusPill(Hs02UiText::kBoost());
  } else if (ready) {
    applyTheme(kReadyC);
    drawStatusPill(Hs02UiText::kReady());
  } else if (cooling) {
    applyTheme(kCoolingC);
    drawStatusPill(Hs02UiText::kCooling());
  } else {
    applyTheme(kHeatingC);
    drawStatusPill(Hs02UiText::kHeating());
  }
  drawSupplyVoltage();
  drawHeroTemperature(current);
  char caption[24];
  drawTarget(boostModeOn ? Hs02UiText::kTarget() : targetCaption(target, caption), target);
  // Heating: progress towards the target. At temperature: heater load vs the power limit, so a
  // big joint pulling the tip down is visible at a glance (like the stock power bar).
  if (ready) {
    drawProgress(powerLoadX1000(x10Watt));
  } else if (cooling) {
    drawProgress(current ? (uint16_t)((uint32_t)target * 1000 / current) : 0);
  } else {
    drawProgress(progressX1000(current, target));
  }
  drawPower(x10Watt);
  drawSleepCountdown();
}

// Boot splash: status pill + version, the model name in the hero face, the segment bar
// filling up and a "Загрузка  73%" footer. Turns green ("Готов") once the bar is full.
void ui_draw_boot_screen(uint16_t progressX1000) {
#if defined(MODEL_HS02A)
  const char *model = "HS-02A";
#elif defined(MODEL_HS02B)
  const char *model = "HS-02B";
#else
  const char *model = "HS-02";
#endif
  if (progressX1000 > 1000) {
    progressX1000 = 1000;
  }
  const bool done = progressX1000 >= 1000;
  applyTheme(done ? kReadyC : kHeatingC);
  drawStatusPill(done ? Hs02UiText::kReady() : Hs02UiText::kBootPill());

  const uint8_t versionW = textWidth(Hs02ModernFont::kLabel, BUILD_VERSION);
  drawText(Hs02ModernFont::kLabel, BUILD_VERSION, kScreenW - kMargin - versionW, topFor(Hs02ModernFont::kLabel, 13), kGrayIdx);

  drawText(Hs02ModernFont::kHero, model, kMargin, topFor(Hs02ModernFont::kHero, kValueBaseline), kInk, kEdgeIdx);
  drawProgress(progressX1000);

  drawFooterHint(Hs02UiText::kBooting());
  char text[6];
  append(formatUnsigned(text, progressX1000 / 10), "%");
  const uint8_t pctW = textWidth(Hs02ModernFont::kValue, text);
  drawText(Hs02ModernFont::kValue, text, kScreenW - kMargin - pctW, topFor(Hs02ModernFont::kValue, kFooterBaseline), kInk, kEdgeIdx);
}

void ui_draw_home_gauge_sleep(TemperatureType_t tipTemp) {
  applyTheme(kSleepC);
  drawStatusPill(Hs02UiText::kSleep());
  drawSupplyVoltage();
  drawHeroTemperature(tipTemp);
  drawTarget(Hs02UiText::kTarget(), getSettingValue(SettingsOptions::SleepTemp));
  drawProgress(0);
  drawFooterHint(Hs02UiText::kHintWake());
}

// ---------------------------------------------------------------- settings menu
namespace {
constexpr uint32_t kMenuAccent = 0x0A84FF; // iOS system blue
ColorBand          menuBands[4];
constexpr uint8_t  kRowTop = 18, kRowH = 20;
} // namespace

void ui_draw_menu(const char *title, uint8_t position, uint8_t count, const Hs02MenuRow *rows, uint8_t rowCount, uint8_t selected, bool editing) {
  const uint16_t black = rgb565(kBlack), white = rgb565(kWhite), gray = rgb565(kGray), edge = rgb565(kEdge);
  const uint16_t accent = rgb565(kMenuAccent), tint = rgb565(mixRgb(kMenuAccent, kBlack, 70));
  menuBands[0] = {0, {black, accent, tint, gray}};
  for (uint8_t r = 0; r < 3; r++) {
    // Row bands: 0 bg, 1 label, 2 edge (or the cursor tint), 3 value (accent on the cursor row).
    menuBands[1 + r] = (r == selected) ? ColorBand{(uint8_t)(kRowTop + r * kRowH), {black, white, tint, accent}}
                                       : ColorBand{(uint8_t)(kRowTop + r * kRowH), {black, white, edge, gray}};
  }
  Display::setColorBands(menuBands, 4);

  drawStatusPill(title);
  char pos[10];
  char *p = formatUnsigned(pos, position + 1);
  *p++    = '/';
  formatUnsigned(p, count);
  const uint8_t posW = textWidth(Hs02ModernFont::kLabel, pos);
  drawText(Hs02ModernFont::kLabel, pos, kScreenW - kMargin - posW, topFor(Hs02ModernFont::kLabel, 13), kGrayIdx);

  for (uint8_t r = 0; r < rowCount && r < 3; r++) {
    const uint8_t y0       = kRowTop + r * kRowH;
    const bool    cursor   = r == selected;
    const uint8_t edgeIdx  = cursor ? kNoEdge : kEdgeIdx;
    const uint8_t baseline = y0 + 14;
    if (cursor) {
      fillRounded(2, y0 + 1, kScreenW - 4, kRowH - 2, 6, kTint);
    }
    drawText(Hs02ModernFont::kMenu, rows[r].label, 9, topFor(Hs02ModernFont::kMenu, baseline), kInk, edgeIdx);
    if (rows[r].flags & HS02_ROW_SUBMENU) {
      const uint8_t w = textWidth(Hs02ModernFont::kMenu, Hs02UiText::kChevron());
      drawText(Hs02ModernFont::kMenu, Hs02UiText::kChevron(), kScreenW - 9 - w, topFor(Hs02ModernFont::kMenu, baseline), kGrayIdx, edgeIdx);
    } else if (rows[r].value && rows[r].value[0]) {
      const uint8_t w = textWidth(Hs02ModernFont::kMenu, rows[r].value);
      if (cursor && editing) {
        // Value being edited: accent capsule with dark text, iOS-style.
        const uint8_t cw = w + 12, cx = kScreenW - 5 - cw;
        fillRounded(cx, y0 + 3, cw, kRowH - 6, (kRowH - 6) / 2, kGrayIdx /* accent in this band */);
        drawText(Hs02ModernFont::kMenu, rows[r].value, cx + 6, topFor(Hs02ModernFont::kMenu, baseline), kBg);
      } else {
        drawText(Hs02ModernFont::kMenu, rows[r].value, kScreenW - 9 - w, topFor(Hs02ModernFont::kMenu, baseline), kGrayIdx, edgeIdx);
      }
    }
  }
}


// Generic colour alert / confirmation: pill title, one white line, one grey line, a hint.
void ui_draw_alert(const char *title, const char *line1, const char *line2, const char *hint, uint32_t accentRgb) {
  const uint16_t black = rgb565(kBlack), white = rgb565(kWhite), gray = rgb565(kGray), edge = rgb565(kEdge);
  menuBands[0]         = {0, {black, rgb565(accentRgb), rgb565(mixRgb(accentRgb, kBlack, 70)), gray}};
  menuBands[1]         = {18, {black, white, edge, gray}};
  menuBands[2]         = {58, {black, white, edge, gray}};
  Display::setColorBands(menuBands, 3);
  drawStatusPill(title);
  drawText(Hs02ModernFont::kMenu, line1, kMargin, topFor(Hs02ModernFont::kMenu, 36), kInk, kEdgeIdx);
  if (line2) {
    drawText(Hs02ModernFont::kLabel, line2, kMargin, topFor(Hs02ModernFont::kLabel, 51), kGrayIdx, kEdgeIdx);
  }
  if (hint) {
    drawText(Hs02ModernFont::kLabel, hint, kMargin, topFor(Hs02ModernFont::kLabel, 73), kGrayIdx, kEdgeIdx);
  }
}

void ui_draw_menu_confirm(const char *title, const char *line1, const char *line2, const char *hint, bool danger) {
  ui_draw_alert(title, line1, line2, hint, danger ? kBoostC : kMenuAccent);
}

// Colour versions of the startup / safety warnings (hooked from warnUser()).
void ui_draw_warning(uint8_t which) {
  switch (which) {
  case HS02_WARN_SETTINGS_RESET:
    ui_draw_alert(Hs02UiText::kMenuTitle(), Hs02UiText::kWarnReset1(), Hs02UiText::kWarnReset2(), Hs02UiText::kAnyKey(), kHeatingC);
    break;
  case HS02_WARN_NO_ACCEL:
    ui_draw_alert(Hs02UiText::kWarnSensor(), Hs02UiText::kWarnAccel1(), Hs02UiText::kWarnAccel2(), Hs02UiText::kAnyKey(), kHeatingC);
    break;
  case HS02_WARN_NO_PD:
    ui_draw_alert(Hs02UiText::kWarnPower(), Hs02UiText::kWarnPD1(), Hs02UiText::kWarnPD2(), Hs02UiText::kAnyKey(), kHeatingC);
    break;
  case HS02_WARN_RUNAWAY:
  default:
    ui_draw_alert(Hs02UiText::kWarnRunaway(), Hs02UiText::kWarnRunaway1(), Hs02UiText::kWarnRunaway2(), Hs02UiText::kWarnUnplug(), kFaultC);
    break;
  }
}

void ui_draw_warning_undervoltage_idle(void) {
  Display::setColorMode(true);
  Display::clearScreenColor();
  char           volts[16];
  const uint32_t v = getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);
  char          *p = formatTenths(volts, v);
  *p++             = ' ';
  append(p, Hs02UiText::kVolt());
  ui_draw_alert(Hs02UiText::kWarnPower(), Hs02UiText::kWarnLowV1(), volts, Hs02UiText::kWarnLowV2(), kFaultC);
}

// Calibration step screen. step 0..2, setpoint = 150/250/350, reading = what the user typed
// from the external thermometer, ready = tip has been stable at the setpoint for a while.
void ui_draw_calibration(uint8_t step, TemperatureType_t current, TemperatureType_t setpoint, TemperatureType_t reading, bool ready) {
  const uint16_t black = rgb565(kBlack), white = rgb565(kWhite), gray = rgb565(kGray), edge = rgb565(kEdge);
  const uint32_t acc   = ready ? kReadyC : kHeatingC;
  const uint16_t a     = rgb565(acc), tint = rgb565(mixRgb(acc, kBlack, 60));
  bands[0]             = {kBandStatusY, {black, a, tint, gray}};
  bands[1]             = {kBandValueY, {black, white, gray, a}}; // hero white, caption grey, reading accent
  bands[2]             = {kBandBarY, {black, a, tint, rgb565(kTrack)}};
  bands[3]             = {kBandFooterY, {black, white, edge, gray}};
  Display::setColorBands(bands, 4);

  char pill[32];
  char *p = append(pill, Hs02UiText::kCalTitle());
  *p++    = ' ';
  *p++    = (char)('1' + step);
  *p++    = '/';
  *p++    = '3';
  *p      = '\0';
  drawStatusPill(pill);
  char sp[8];
  append(formatUnsigned(sp, setpoint), "°");
  const uint8_t spW = textWidth(Hs02ModernFont::kLabel, sp);
  drawText(Hs02ModernFont::kLabel, sp, kScreenW - kMargin - spW, topFor(Hs02ModernFont::kLabel, 13), kGrayIdx);

  char text[8];
  formatUnsigned(text, current > 999 ? 999 : current);
  const uint8_t end = drawText(Hs02ModernFont::kHero, text, kMargin, topFor(Hs02ModernFont::kHero, kValueBaseline), kInk);
  drawText(Hs02ModernFont::kValue, "°", end + 1, 20, 2);

  append(formatUnsigned(text, reading), "°");
  const uint8_t vW = textWidth(Hs02ModernFont::kValue, text);
  drawText(Hs02ModernFont::kValue, text, kScreenW - kMargin - vW, topFor(Hs02ModernFont::kValue, kValueBaseline), 3);
  const uint8_t cW = textWidth(Hs02ModernFont::kLabel, Hs02UiText::kCalThermo());
  drawText(Hs02ModernFont::kLabel, Hs02UiText::kCalThermo(), kScreenW - kMargin - cW, topFor(Hs02ModernFont::kLabel, 30), 2);

  drawProgress(ready ? 1000 : progressX1000(current, setpoint));
  drawFooterHint(ready ? Hs02UiText::kCalSet() : Hs02UiText::kCalWait());
}

#endif
