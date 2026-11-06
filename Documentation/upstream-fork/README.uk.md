# IronOS — FNIRSI HS-02B

IronOS-порт для паяльника **FNIRSI HS-02B** (картриджі **C210**, мікрожала).

Репозиторій: <https://github.com/MYMDO/IronOS-FNIRSI-HS-02B>

Форк проєктів:

* [Ralim — IronOS](https://github.com/Ralim/IronOS) (апстрим)
* [PanKleszcz — IronOS](https://github.com/PanKleszcz/IronOS) (`fnirsi_hs02` порт)
* [ok2cm — IronOS](https://github.com/ok2cm/IronOS) (кольоровий UI 160×80, PID, boot logo)

> Статус: HS-02B збирається як окрема модель `HS02B` (`source/Makefile`: `ALL_FNIRSI_MODELS=HS02 HS02A HS02B`).
> HS-02 / HS-02A збережено для сумісності, але вся документація цього репозиторію орієнтована саме на **HS-02B**.

## Відмінність HS-02B від HS-02 / HS-02A

| Параметр | HS-02 / HS-02A | **HS-02B (цей репо)** |
| :-- | :-- | :-- |
| Картридж | C245 (~2.5 Ом) | **C210 (2.5–2.7 Ом, номінал 2.6 Ом)** |
| `TIP_RESISTANCE` | 25 | **26** |
| PID | KP=55, KI=700, KD=6000 | **KP=40, KI=700, KD=4500** |
| `TIP_PID_INTEGRAL_LIMIT_SCALE` | 30 | **30** |
| Ліміт потужності | 60 Вт за замовч., макс. 100 Вт | **60 Вт за замовч., макс. 100 Вт** |
| Модель збірки | `model=HS02` / `model=HS02A` | **`model=HS02B`** |
| Файл прошивки | `HS02A_EN_firmware.bin` | **`HS02B_EN_firmware.bin`** |

Решта заліза спільна для всієї лінійки HS-02 (див. [Hardware](#апаратна-платформа-hs-02b)).

## Апаратна платформа HS-02B

Факти з `source/Core/BSP/Fnirsi/` (схема + реверс стокової прошивки):

* **MCU:** Nation **N32L403** (Cortex-M4, 64 МГц, STM32F1-сумісна периферія).
  Адреса застосунку `0x08005800` — перші `0x5800` зайняті FNIRSI-bootloader'ом.
* **Дисплей:** 160×80 кольоровий IPS, **ST7735** по SPI @ 16 МГц.
  Драйвер `Core/Drivers/LCD.cpp` (`LCD_160x80`), низькорівневий SPI — `Core/BSP/Fnirsi/SPI_Wrapper.cpp`.
  UI-варіант `color_160x80` (`Core/Threads/UI/drawing/color_160x80/`).
* **Акселерометр:** Kionix **KXTJ3-1057**, програмний (bit-bang) I2C: PA9=SCL / PA10=SDA, адреса `0x0E`, без INT (опитування).
* **Нагрів:** P-MOSFET AO4407A, ШІМ на PA8 (`PWR_OUT`); шунт струму 10 мОм → PB1 (`I_MEAS`);
  термопара жала → AD8552 → PA0 (`T_MEAS`), читання через ADC+DMA.
* **USB-PD:** sink-контролер **CH224K**. MCU задає tier напруги через PB6/PB7 (`CFG`),
  USB-маршрут — PD15. Запит фіксовано **20 В** (зворотного зв'язку про домовлену напругу від CH224K немає).
* **Кнопки (3 шт., активний низький, pull-up):** UP=PB4, DOWN=PB5, OK=PB3.
  В IronOS дві логічні кнопки мапляться на UP/DOWN, а OK доступний як `getButtonOK()`
  (`BUTTON_OK_SHORT/LONG`): коротке — enter/confirm, довге — назад.
* **Flash-розкладка:** налаштування IronOS — від `0x0801E000` (`SETTINGS_START_PAGE`, 120 К);
  логотип — `0x08000000 + 122 К`; стокова сторінка калібрування `0x0801F800` **не затирається**
  і перевикористовується (див. [Калібрування](#калібрування-температури)).

## Ключові можливості

* PID-регулятор температури (`TIP_CONTROL_PID`) з умовним anti-windup у спільному `PIDThread.cpp`.
* Кольоровий UI 160×80: home / soldering / boost / sleep.
* Кольоровий boot logo (`FnirsiBootLogo.h`, `LCD::drawNativeImage()`).
* Акселерометр (сон/пробудження за рухом, автоворот екрана).
* Статусний LED, бузер (за замовчуванням гучність 0 — `BUZZER_VOLUME 0`).
* Детальні екрани solder/idle увімкнені за замовчуванням (`DETAILED_SOLDERING 1`, `DETAILED_IDLE 1`).
* DC + PD живлення (QC немає — апаратно відсутній).

Попередній перегляд UI без заліза (рендер production-коду через WebAssembly):

```bash
tools/hs02-wasm-preview/build.sh
python3 -m http.server 18080 --directory tools/hs02-wasm-preview
# відкрити http://127.0.0.1:18080/web/ — idle / solder / boost / sleep
```

## Обмеження

* Немає вибору запитуваної PD-напруги (зафіксовано 20 В через CH224K).
* Немає детекції опору жала / захисту від КЗ нагрівника в релізі.
* Немає підтримки boot logo з меню (логотип компілюється в прошивку).
* Переклади не тестувались — гарантовано тільки EN (збираються також інші `translation_*.json`, напр. UK).
* Порти оригінального IronOS (TS100/TS80/Pinecil/…) у дереві збережено, але **не тестуються** на цьому форку.

## Прошивка HS-02B

### Що завантажувати

Після збірки потрібен файл з суфіксом **`_firmware.bin`** — він містить 16-байтовий `bin`-заголовок
для MSD-bootloader'а (`source/Core/BSP/Fnirsi/Scripts/add_header.py`):

* `source/Hexfile/HS02B_EN_firmware.bin` — англійська
* `source/Hexfile/HS02B_UK_firmware.bin` — українська (якщо зібрана)

> Звичайний `HS02B_EN.bin` **не прошиється** — завжди беріть `*_firmware.bin`.

### Кроки прошивки

1. Утримуйте кнопку **OK** і підключіть HS-02B кабелем USB до ПК.
2. Відпустіть кнопку, коли з'явиться `Please drag the firmware file to the USB flash disk!`.
3. Дочекайтесь MSD-накопичувача **BOOTLOADER** з порожнім файлом `READY.TXT`.
4. Скопіюйте `HS02B_*_firmware.bin` на `BOOTLOADER` (автоматично: `./scripts/flash_hs02b_linux.sh source/Hexfile/HS02B_EN_firmware.bin`).
   1. На Linux скрипт/вручну копіює файл двічі (з `sync` після кожної копії) — bootloader FNIRSI
      некоректно працює з USB write caching, інакше паяльник зависне в bootloader'і з напівзаписаним образом.
   2. Є таймаут ~40 с на початок завантаження — після нього bootloader виходить у поточну прошивку.
5. З'явиться `Updating firmware!`, пристрій перезавантажиться в нову прошивку.

Детально: [Documentation/Flashing/HS02B.md](Documentation/Flashing/HS02B.md).

## Збірка HS-02B

Вимоги: `arm-none-eabi-gcc`, `pip install bdflib` (генерація шрифтів/перекладів).

```bash
cd source
make model=HS02B -j$(nproc)
# EN за замовчуванням: make model=HS02B firmware-EN
# Українська:
make model=HS02B firmware-UK -j$(nproc)
```

Результат — `source/Hexfile/HS02B_*_firmware.bin` (+ `.hex`, `.dfu`, `.elf`, `.map`).

Мультимовні/інші цілі — як в апстримі (`make list` у корені, `source/Makefile`).

Перевірки стилю перед PR:

```bash
make -C source/ clean check-style
```

## Калібрування температури

Модель — `source/Core/BSP/Fnirsi/ThermoModel.cpp`:

1. **Заводське калібрування стоку (пріоритет):** стокова прошивка тримає 3 слова
   користувацького калібрування на сторінці `0x0801F800` (слова `0x16/0x17/0x18`,
   `count = word − bias`, bias `0x7F21/0x7E77/0x7DC4`, якорі кривої 140/240/340 °C).
   Плейсхолдер `0x8000` («ніколи не калібрувалось») **відхиляється**.
   Налаштування IronOS лежать нижче `0x0801F000`, тому стокова сторінка переживає перепрошивку.
2. **Fallback:** виміряна стала ~26 мкВ/°C (калібровано по точках плавлення припою
   Sn45/Pb55: solidus ~183 °C / liquidus ~227 °C) — приблизно в 3 рази точніше за некалібрований сток.

Сторінка Debug menu **Tip Cal** (17-та сторінка; з home — довге утримання UP)
показує три декодовані counts + `In Use` / `Unused 26uV/C`.

Шлях без змін коду для точності ±3 °C: прошийте сток → відкалібруйте за термометром жала
на екрані 150/250/350 °C → перепрошийте IronOS (значення підхопляться автоматично).
Деталі — `TODO.md`, `CLAUDE.md`.

## Керування (3 кнопки HS-02B)

* `UP` (біля жала) / `DOWN` (біля хвоста): коротке — ±1 °C, довге — ±10 °C / навігація.
* `OK`: коротке — вхід/підтвердження, довге — назад.
* Soldering: довге UP — **Boost** (тимчасовий `BOOST_TEMP`, за замовч. 420 °C);
  довге DOWN — вихід у standby; обидві довго — lock.
* Standby: довге UP — екран встановлення температури до нагріву; довге DOWN — debug menu.
* Home: довге DOWN — debug menu.

Повний опис меню — апстрим-документація (`Documentation/Menu.md`, `Settings.md`,
`GettingStarted.md`) + [HS-02B flashing guide](Documentation/Flashing/HS02B.md).

## Документація

* [Getting Started (HS-02B notes)](Documentation/GettingStarted_HS02B.md)
* [Flashing HS-02B](Documentation/Flashing/HS02B.md)
* [Hardware notes](Documentation/Hardware.md) (+ [Port BSP README](source/Core/BSP/Fnirsi/README.md))
* [Історія/ TODO](TODO.md) · [Нотатки розробника](CLAUDE.md)
* Решта `Documentation/*.md` — апстрим IronOS (Pinecil/TS100/…), застосовна частково.

## Ліцензія

Код спільноти — [GNU GPLv3](LICENSE), якщо не вказано інше.
FreeRTOS / USB-PD та інші компоненти — під власними ліцензіями.

Цей форк надається **«AS IS»**, без комерційної підтримки.
Не ре-хостіть файли — давайте посилання на цей репозиторій / релізи, щоб не розпорошувати старі версії.
