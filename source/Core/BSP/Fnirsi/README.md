# BSP — N32L403 based FNIRSI HS-02B

Hardware abstraction for the **FNIRSI HS-02B** (C210 cartridges).
Same BSP also builds `HS02` / `HS02A` (C245) — see `configuration.h`.

## HS-02B specifics

* MCU Nation N32L403 (Cortex-M4 64 МГц), link at `0x08005800` (`Scripts/n32l403_flash_user.ld`).
* Display ST7735 160×80, SPI1 16 МГц (`SPI_Wrapper.cpp`, `LCD_Port.hpp`).
* Accel KXTJ3-1057, software I2C PA9/PA10 (`Software_I2C.h`).
* Heater PWM PA8 / current PB1 / thermocouple PA0 (`Pins.h`, `BSP.cpp`, `ThermoModel.cpp`).
* PD CH224K, fixed 20 V (`Setup.cpp`: `USB_CTL PD15`, `CH224_CFG PB6/PB7`).
* Buttons UP PB4 / DOWN PB5 / OK PB3 (`getButtonA/B/OK` in `BSP.cpp`).
* HS-02B tuning (`configuration.h`): `TIP_RESISTANCE 26`, `TIP_PID_KP 40`,
  `TIP_PID_KD 4500` (C245: 25 / 55 / 6000). Common: `TIP_PID_KI 700`,
  `TIP_PID_INTEGRAL_LIMIT_SCALE 30`, `POWER_LIMIT 60`, `MAX_POWER_LIMIT 100`.
* Flash: settings `0x0801E000`, logo `0x08000000+122K`, stock cal page `0x0801F800` preserved.
* Firmware header: `Scripts/add_header.py` (16-byte `bin` header → `*_firmware.bin`).

Build: `cd source && make model=HS02B -j$(nproc)`.
Flash: hold OK + USB → `BOOTLOADER` MSD → copy `HS02B_*_firmware.bin` **twice** + `sync` on Linux.
