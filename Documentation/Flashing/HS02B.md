# Flashing — FNIRSI HS-02B

Ця інструкція — тільки для **FNIRSI HS-02B** (C210) з прошивками `HS02B_*_firmware.bin`
цього репозиторію (<https://github.com/MYMDO/IronOS-FNIRSI-HS-02B>).
Для інших паяльників дивіться апстрим-гайди (MHP30 / Pinecil / TS100 / TS80).

## Який файл брати

З `source/Hexfile/` після `make model=HS02B`:

| Файл | Призначення |
| :--- | :--- |
| `HS02B_EN_firmware.bin` | **Прошивати цей** (EN, з 16-байтовим заголовком для MSD-bootloader'а) |
| `HS02B_UK_firmware.bin` | **Прошивати цей** (UK, якщо зібрано) |
| `HS02B_EN.bin` | НЕ прошиється (без заголовка) — тільки артефакт збірки |
| `HS02B_EN.hex` / `.dfu` / `.elf` | Для відлагодження, не для MSD-прошивки |

Заголовок додає `source/Core/BSP/Fnirsi/Scripts/add_header.py` (`CUSTOM_FORMAT_CMD`
у `source/Makefile` для всіх `ALL_FNIRSI_MODELS=HS02 HS02A HS02B`).

## Вхід у bootloader

1. Утримуйте кнопку **OK** (середня/нижня, PB3) і підключіть HS-02B USB-кабелем до ПК.
2. Відпустіть OK, коли на екрані `Please drag the firmware file to the USB flash disk!`.
3. У системі з'явиться знімний диск **BOOTLOADER** з порожнім файлом `READY.TXT`.
   Якщо диска немає — перевірте кабель (має бути data, не charge-only) і повторіть з п.1.

## Копіювання прошивки

Автоматично (рекомендовано):

```bash
./scripts/flash_hs02b_linux.sh source/Hexfile/HS02B_EN_firmware.bin
```

Скрипт сам знайде змонтований `BOOTLOADER` (або прийміть точку монтування другим
аргументом), скопіює файл **двічі** з `sync` і перевірить ім'я `HS02B_*_firmware.bin`.
Вручну — ті самі кроки:

1. Скопіюйте `HS02B_*_firmware.bin` у корінь диска `BOOTLOADER`.
2. **Linux: скопіюйте той самий файл ще раз** і виконайте `sync` після кожної копії:

   ```bash
   cp HS02B_EN_firmware.bin /media/$USER/BOOTLOADER/
   sync
   cp HS02B_EN_firmware.bin /media/$USER/BOOTLOADER/
   sync
   ```

   Причина: MSD-bootloader FNIRSI некоректно працює з USB write caching — з однією копією
   паяльник може зависнути в bootloader'і з напівзаписаним образом.
3. Встигніть почати копіювання за ~40 с — інакше bootloader вийде у поточну прошивку
   і процедуру доведеться повторити.
4. Дочекайтесь `Updating firmware!` — пристрій сам перезавантажиться в IronOS.

## Після прошивки

* При першому старті можливе попередження про скидання налаштувань — це нормально
  (структура `Settings` змінилась). Налаштування зберігаються від `0x0801E000`.
* Стокова сторінка калібрування `0x0801F800` не затирається — заводська калібровка
  температури підхопиться автоматично (Debug menu → Tip Cal покаже `In Use`).
* Якщо бачите `X` замість жала — перевірте посадку картриджа C210; при живленні <9 В
  (напр. 5 В USB) детект відключення жала свідомо вимкнений (`isTipDisconnected()`),
  бо ланцюг термопари не живиться коректно.

## Повернення на сток

Процедура та сама — скопіюйте офіційний `APP_HS_02B.bin`-подібний образ FNIRSI
на `BOOTLOADER` (двічі + `sync` на Linux). IronOS-налаштування при цьому залишаться
у `0x0801E000` і не зачеплять стокову сторінку `0x0801F800`.

## Збірка з вихідників

```bash
cd source
make model=HS02B -j$(nproc)              # EN
make model=HS02B firmware-UK -j$(nproc)  # UK
ls -lh Hexfile/HS02B_*_firmware.bin
```

Вимоги: `arm-none-eabi-gcc`, `pip install bdflib`.
