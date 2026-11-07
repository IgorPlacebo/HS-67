# Changelog

All notable changes of **HS-02 Modern UI** (on top of [MYMDO/IronOS-FNIRSI-HS-02B](https://github.com/MYMDO/IronOS-FNIRSI-HS-02B)).

## [Unreleased]

### Added
- **Power-on melody**: a ~3 s tune (`tools/hs02-ui-sounds/startup.rtttl`, baked by
  `gen_startup_melody.py`) plays with the boot splash at the *Sound* volume; skipping the splash or
  any UI beep stops it. TIM2 now ticks at 1 MHz so the buzzer pitch can change per note; the player
  runs from the TIM2 update interrupt.
- **Heating screen with a heart**: a pink heart with a dark heart inside that beats every 1.2 s,
  the device name (*ЭмОчка*, new 20 px Title face) and "греемся… 56%" below. Cooling uses the same
  screen ("остываем…").
- **Ready screen with a picture per preset**: when the target is preset 1, 2 or 3 and the tip is
  at temperature, that preset's picture fills the top 64 rows; a strip below shows the tip
  temperature, the preset and the heater power. Pictures live in `tools/hs02-ui-images/presets/`
  and are baked by `gen_preset_images.py` (160x64, 4 colours per 4-row band, ~2.7 KB each).
  A custom target (not a preset) and boost keep the classic gauge.
  The pictures are black-and-white portraits cut out of a group photo (background removed).
- **Standby screen with a picture**: after power-on (heater off, tip below 50 °C) the standby
  picture (`presets/standby.png`) fills the screen, with "● 27°" and "OK — нагрев" below. A hot tip
  still shows the blinking *Hot!* gauge.

### Changed
- **Names**: the boot splash shows *HS-67* (both HS-02A and HS-02B builds) and, in Russian, the
  pill *ЭмОчка*; the Russian sleep pill reads *ZzZ*.
- **Palette**: pink is the system colour and means "ready" (was green for ready, blue for the
  menu); standby and sleep are purple (were grey / cyan); heating, cooling and the startup warnings
  are a pale purple (were orange / teal); faults, boost and *Hot!* are a clear red.
- **Typeface: Oswald** (condensed, SIL OFL) instead of Inter on every colour screen: hero digits
  Oswald SemiBold 36 px, values Medium 16 px, labels Medium 12 px (was 10 px), menu Medium 13 px.
  Glyph solids now come from the anti-aliased coverage (Oswald's hinted mono render breaks stems at
  these sizes). The value being edited in the menu gets a soft edge so its digits stay legible.

### Removed
- **Legacy IronOS screens** on HS-02: the classic settings menu, diagnostics, USB-PD debug, CJC
  calibration, temperature-adjust and profile screens, and the *Advanced* menu that led to them.
  Their code and the legacy fonts are no longer linked in (~23 KB of flash freed). Settings that
  were only reachable there keep their saved values; *Factory reset* restores the defaults.

## [1.0.1] — 2026-11-06

### Fixed
- **False "Shorted" / "No tip"** on irons whose cold tip reads ~0 µV (amplifier offset differs
  between units): a near-zero reading no longer means short or missing tip, so heating is no longer
  blocked. "No tip" is now reported for a saturated (open) thermocouple, or when the heater has been
  driven ≥ 10 W for 2 s without the reading leaving zero.
- Standby shows a clear "Low input voltage" alert below 9 V (5 V USB / non-PD supply) instead of
  misleading tip states.

## [1.0.0] — 2026-11-06

### Added
- **New colour UI** (`draw_home_gauge.cpp`): status pill, Inter typeface with 2-level anti-aliasing,
  segmented bar; states Standby, Heating, Ready, Cooling, Hot!, Sleep, No tip / Shorted.
- **Per-row palettes** for the 2bpp framebuffer (`ColorBand.hpp`, `LCD::setColorBands`): 4 colours per
  screen band instead of 4 per screen, no extra RAM.
- **Animated boot splash** drawn in code (replaces the 25 KB bitmap logo).
- **Settings menu** in the stock FNIRSI structure: Sleep / Tip / System / Advanced, English and Russian
  (switchable at run time, *Auto* follows the build language).
- **FNIRSI-style controls**: OK = heat, +/− = target with accelerating hold, OK = presets 1-3,
  hold OK = stop / settings; deferred, wear-friendly saving of the target.
- **3-point tip calibration** (150/250/350 °C) in the stock calibration format, with a factory backup
  and *Reset calibration*.
- **Buzzer volume** setting (0-10) and chimes (ready, presets, start/stop).
- **USB-PD voltage** setting (9/12/15/20 V via CH224K CFG2/CFG3).
- **Firmware update from the menu** using the stock bootloader flag (`0x87456321` @ `0x0801F800`, word 18).
- **Colour alerts** for settings reset, missing accelerometer / PD, thermal runaway and low voltage.
- Heater-load bar at temperature (like the stock power bar).
- Tools: Inter font generator, native screenshot renderer, CI building HS02A/HS02B × EN/RU with releases.

### Changed
- Default sleep timeout 50 s → 5 min, +/− step 1 → 5 °C (also migrated on upgrade).
- `TranslationLanguageCode` symbol emitted by `make_translation.py`.

### Fixed
- **HS-02A bootloader header**: lowercase `bin` tag (the HS-02B fork wrote `BIN`, which the HS-02A
  bootloader ignores). Tag chosen per model via `FNIRSI_HEADER_TAG`.
- Buzzer was compiled with volume 0 (silent).
