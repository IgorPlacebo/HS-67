# Changelog

All notable changes of **HS-02 Modern UI** (on top of [MYMDO/IronOS-FNIRSI-HS-02B](https://github.com/MYMDO/IronOS-FNIRSI-HS-02B)).

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
