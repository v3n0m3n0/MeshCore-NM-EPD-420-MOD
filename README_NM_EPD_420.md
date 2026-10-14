# MeshCore-NM-EPD-420 v1.17.1 Companion BLE

This repository is an optimized, hardware-verified fork of [MeshCore](https://github.com/meshcore-dev/MeshCore) providing full, production-ready support for the **RockBase NM-EPD-420** ESP32-S3 board with a **4.2-inch Black & White E-Paper display (GYE042A87 / SSD1683)** and **Semtech SX1262 LoRa transceiver**.

---

## Hardware Overview

| Subsystem | Component | Specifications & Details |
|---|---|---|
| **MCU** | ESP32-S3R8 | Dual-Core Xtensa LX7 @ 80–240 MHz, **8 MB Octal PSRAM (OPI)**, **16 MB Quad Flash (QIO)** |
| **Display** | 4.2" E-Paper | GYE042A87 panel (400 × 300 px), SSD1683 controller, 1-bit B/W, SPI interface |
| **Radio (LoRa)** | Semtech SX1262 | Heltec HT-RA62 footprint, 868 / 915 MHz, native **+22 dBm** (+160 mW) max output |
| **RF Switching** | Internal DIO2 | `SX126X_DIO2_AS_RF_SWITCH=true`, 1.8V TCXO on DIO3 |
| **Sensors** | AHT20 | Temperature & relative humidity (I2C `0x38`), gated via `TEMP_CTL` (IO40) |
| **Audio** | ES8311 + NS4150B | I2S audio codec (`0x18`) + Class-D speaker amp gated via `PA_CTRL` (IO41), PDM mic |
| **Power & Battery**| LiPo Charger / ADC | USB-C charging, battery voltage divider 2:1 on ADC IO3 (gated via `ADC_EN` IO43) |
| **Buttons** | Physical Buttons | `BOOT` = IO0 (RTC GPIO, active LOW), `USER` = IO45 (active LOW) |

---

## Key Features & Fixes in This Fork

### 1. Modern PlatformIO Toolchain & Memory Mapping (`qio_opi`)
* Uses `pioarduino 54.03.21` (Arduino-ESP32 3.2.1 + ESP-IDF 5.3) to resolve octal PSRAM boot-loops present in older Expressif32 cores.
* Correctly configured memory flags: `board_build.arduino.memory_type = qio_opi` and 16 MB flash partition table (`default_16MB.csv`).
* Automated unified binary script [`merge-bin.py`](merge-bin.py) creating a single 0x00000000 flashable image (`firmware-merged.bin`) containing bootloader, partitions, otadata, and application binary.

### 2. Tailored 4.2" E-Paper UI Layout
* Adapted the `ui-new` screen architecture for the 400×300 canvas.
* Fixed baseline alignment and vertical spacing across the Home, Radio, Bluetooth, and Advert screens, preventing text collisions observed with smaller default displays.
* Integrated hardware button navigation:
  * `BOOT` button (IO0): Short press advances pages (`KEY_NEXT`), double-click selects (`KEY_SELECT`), long press confirms (`KEY_ENTER`).
  * `USER` button (IO45): Short press navigates backwards (`KEY_PREV`).

### 3. Polish UTF-8 Diacritics Transliteration Engine
* Resolved an issue where non-ASCII characters were silently dropped by vector font renderers (`Adafruit_GFX` / `FreeSans`), causing words like `"żaba"` to display as `"aba"`.
* Implemented a zero-allocation transliteration filter in [`src/helpers/ui/DisplayDriver.h`](src/helpers/ui/DisplayDriver.h) mapping all 18 Polish diacritics (`ą, ć, ę, ł, ń, ó, ś, ź, ż` / `Ą, Ć, Ę, Ł, Ń, Ó, Ś, Ź, Ż`) as well as European typographic marks (curved quotes, dashes, ellipsis) directly to clean ASCII equivalents.

### 4. Persistent Bluetooth LE Bonding & Secure Factory Reset
* Fixed repeated PIN pairing prompts across power cycles by eliminating aggressive NVS bond erasure in [`SerialBLEInterface::begin()`](src/helpers/esp32/SerialBLEInterface.cpp).
* BLE PIN is randomly generated once upon first initialization and persisted permanently in `_prefs.ble_pin` until explicitly reset.
* Three factory reset mechanisms:
  * **App Reset:** Over BLE via companion app (`CMD_FACTORY_RESET`).
  * **CLI Reset:** Serial terminal command `erase`.
  * **Hardware Fail-Safe:** Hold the physical `USER` button (IO45) for **5 seconds** during device power-up to wipe all BLE bonds, erase LittleFS configuration, and restore factory defaults.

### 5. RF Power Verification (SX1262 Max 22 dBm)
* Configured default and maximum transmit power to **+22 dBm** (`-D LORA_TX_POWER=22`, `-D MAX_LORA_TX_POWER=22`).
* Verified against hardware schematics: The board uses a bare Semtech SX1262 (HT-RA62) without an external RF Front-End Module (FEM) like the KCT8103L (which provides ~28 dBm on some other Heltec models). The RadioLib driver enforces a silicon ceiling of +22 dBm for safe, stable transmission.

### 6. Offline Message Queue Architecture (`MSG: 0`)
* The home screen `MSG: %d` counter reflects the state of the radio's **offline buffer (`offline_queue_len`)**.
* When the paired phone is connected via BLE, incoming messages are immediately streamed to the mobile client via `PUSH_CODE_MSG_WAITING` and `CMD_SYNC_NEXT_MESSAGE`, draining the radio's offline queue to 0 in ~20 ms.
* When the phone is disconnected, the device queues incoming messages in local memory and increments `MSG: %d` until reconnected.

---

## Pin Mapping Summary

```
FSPI (EPD Display):
  SCK   -> IO2
  MOSI  -> IO1
  CS    -> IO46
  DC    -> IO4
  RST   -> IO5
  BUSY  -> IO6

HSPI (SX1262 LoRa + MicroSD shared bus):
  SCK   -> IO9
  MOSI  -> IO10
  MISO  -> IO11
  NSS   -> IO8
  RESET -> IO12
  BUSY  -> IO13
  DIO1  -> IO14
  SD_CS -> IO7

I2C Bus (AHT20 + ES8311):
  SDA   -> IO39
  SCL   -> IO38

Power Gating (Active HIGH):
  TEMP_CTL -> IO40  (AHT20 sensor power)
  PA_CTRL  -> IO41  (NS4150B audio amplifier)
  ADC_EN   -> IO43  (Battery voltage measurement)
  CODEC_EN -> IO44  (ES8311 audio codec power)
  LORA_EN  -> IO47  (SX1262 radio power)

Buttons & Battery:
  BOOT_BTN -> IO0   (Active LOW, external pull-up)
  USER_BTN -> IO45  (Active LOW, external pull-up)
  BATT_ADC -> IO3   (ADC1_CH2, 2:1 voltage divider)
```

---

## Building and Flashing

### Environment
* PlatformIO Core (`pio` 6.x+)
* Target environment: **`NM_EPD_420_BW_companion_radio_ble`**

### Build Single Merged Firmware Image
```bash
pio run -e NM_EPD_420_BW_companion_radio_ble -t mergebin
```
The unified binary will be generated at:
`.pio/build/NM_EPD_420_BW_companion_radio_ble/firmware-merged.bin`

### Flash to Device (e.g., COM15 on Windows or /dev/ttyACM0 on Linux)
```bash
python -m esptool --port COM15 --chip esp32s3 write_flash 0x0 .pio/build/NM_EPD_420_BW_companion_radio_ble/firmware-merged.bin
```

### Critical ESP32-S3 Watchdog Reset Fix
Because ESP32-S3 USB-Serial/JTAG can remain latched in download mode after an upload, trigger a clean software restart using:
```bash
python -m esptool --port COM15 --before no-reset --after watchdog-reset flash_id
```

---

## Upstream References and Credits

* **Upstream Project:** [meshcore-dev/MeshCore](https://github.com/meshcore-dev/MeshCore) — The original decentralized mesh networking protocol and firmware.
* **Vendor Reference Fork:** [RockBase-iot/meshcore (branch nm-epd-420)](https://github.com/RockBase-iot/meshcore/tree/nm-epd-420) — Initial vendor bring-up branch.
* **Hardware & Schematics Repository:** [RockBase-iot/NM-EPD-420](https://github.com/RockBase-iot/NM-EPD-420) — Official schematics, pin maps, and hardware documentation.
* **Maintainer of this Fork:** [@v3n0m3n0](https://github.com/v3n0m3n0)

---

## License

This project is licensed under the MIT License, consistent with upstream MeshCore. See [`license.txt`](license.txt) for full details.
