# MeshCore-NM-EPD-420-BW v1.18.0-mod-rev0.2 Firmware Suite Release

This directory contains pre-compiled, hardware-verified unified `0x0` binaries for the **RockBase NM-EPD-420-BW** board (ESP32-S3R8 + 4.2" 400×300 E-Paper + SX1262 LoRa + AHT20), covering **all 5 official MeshCore roles**.

---

## 📦 Available Firmware Roles (-MOD Suite)

Every role build inherits from the unified `-MOD` hardware foundation (`NMEPD420Board`, `NMEPD420Display`, `NMEPD420EnvironmentSensorManager`, `CommonRadioPrefs`):

| Role | Unified Binary (0x0) | Size | Primary Use Case | Hardware Features Active |
| --- | --- | :---: | --- | --- |
| **Companion Radio BLE** | `MeshCore-NM-EPD-420-BW-companion_radio_ble-v1.18.0-mod-rev0.2.bin`<br>*(also aliased as `MeshCore-NM-EPD-420-BW-v1.18.0-mod-rev0.2.bin`)* | ~1.96 MB | Mobile companion (iOS & Android app via BLE, WiFi station, CLI rescue) | Async E-Paper Core 0 (400×300), BLE + WiFi, 256-msg PSRAM queue, AHT20, Battery ADC, 5s factory reset |
| **Companion Radio USB** | `MeshCore-NM-EPD-420-BW-companion_radio_usb-v1.18.0-mod-rev0.2.bin` | ~1.41 MB | Desktop / Laptop companion via USB CDC serial terminal or web app | Async E-Paper Core 0 (400×300), USB CDC, 256-msg PSRAM queue, AHT20, Battery ADC |
| **Repeater** | `MeshCore-NM-EPD-420-BW-repeater-v1.18.0-mod-rev0.2.bin` | ~1.56 MB | Autonomous field / solar relay node extending mesh coverage | Async E-Paper status display, 80 MHz low-power MCU, MicroSD CS isolation, TCXO SX1262 (+22 dBm) |
| **Room Server** | `MeshCore-NM-EPD-420-BW-room_server-v1.18.0-mod-rev0.2.bin` | ~1.56 MB | Micro-BBS room server for shared channels, noticeboards & posts | Async E-Paper status display, 80 MHz low-power MCU, MicroSD CS isolation, TCXO SX1262 (+22 dBm) |
| **KISS Modem** | `MeshCore-NM-EPD-420-BW-kiss_modem-v1.18.0-mod-rev0.2.bin` | ~1.30 MB | Serial KISS TNC bridge for AX.25, APRS, TCP/IP, or Python hosts | Pure headless radio bridge, audio PA shutdown, 80 MHz low-power MCU, MicroSD CS isolation |

---

## ⚡ What's New in v1.18.0-mod-rev0.2

- **Deep Sleep GPIO Hold Latching & LoRa Power Cut:** Proper digital pad hold latching (`gpio_deep_sleep_hold_en()`, `gpio_hold_en()` / `gpio_hold_dis()`) for non-RTC GPIOs (GPIO 40, 41, 43, 44, 47). Fully cuts power to SX1262 LoRa (`SX126X_POWER_EN` IO47) in deep sleep and latches SPI CS lines high to eliminate sleep leakage.
- **Safe I2C Power Sequencing (ESD Clamping Prevention):** Powers on peripheral rails (`CODEC_EN` IO44, `TEMP_CTL` IO40) before `ESP32Board::begin()` initializes I2C lines (IO39/38), eliminating parasitic ESD diode clamping to GND.
- **LiPo Battery Protection & ADC Drain Elimination:** Added `AUTO_SHUTDOWN_MILLIVOLTS=3300` (safe cutoff), `BATT_MIN_MILLIVOLTS=3000`, `BATT_MAX_MILLIVOLTS=4200`. Symmetric immediate turn-off of `PIN_ADC_CTRL` (GPIO 43) after 20 ms RC stabilization and 8-sample ADC read stops continuous battery drain through the voltage divider.
- **Native AHT20 I2C Protocol (Command 0xBE):** Replaced faulty Adafruit AHTX0 initialization (`0xE1`) with direct native I2C implementation sending proper AHT20 calibration command `[0xBE, 0x08, 0x00]`.

---

## ⚡ What's New in v1.18.0-mod-rev0.1

- **Upstream v1.18.0 Synchronization:** Merged official MeshCore Companion Firmware v1.18.0 (commit `81dbae5`, protocol v14).
- **Interactive Companion CLI (`CMD_RUN_CLI_COMMAND`):** Full support for device querying, configuration inspection, dynamic WiFi management, and rescue CLI operations.
- **Node Discover Screen (`HomePage::DISCOVERY`):** Zero-hop repeater and node discovery UI integrated into the native 400×300 layout (`row_step = 14`, UTF-8 transliteration, and `discover: press Enter` prompt).
- **Abstract UI Overhaul (`MyMesh::Listener`):** Migrated UI notifications and message events to the decoupled `MyMesh::Listener` architecture (`onMessageRecv`, `onChannelMessageRecv`, `onQueueSizeChanged`, `onDiscoveredContact`, `onControlDataRecv`).
- **Dynamic WiFi & BLE Coexistence:** Flexible serial interface manager supporting on-demand WiFi station connectivity, automatic link reconnection, and IP address display on the primary home screen when enabled.
- **Full Role Suite Coverage:** All 5 PlatformIO environments compiled and merged into standalone single-file `0x0` images ready for flashing.

---

## 🛡️ Preserved Hardware Optimizations (Shared Across All Roles)

- **Asynchronous Core 0 FreeRTOS E-Paper Task & 16 MHz FSPI:** Offloaded blocking `GxEPD2` screen updates (~600 ms partial / ~2.2s full) to a dedicated FreeRTOS task on Core 0 and raised FSPI clock from 4 MHz to 16 MHz. Frame commit latency on the main thread is <1 ms.
- **Shared HSPI MicroSD Guard (`PIN_SDCARD_CS` IO7):** Explicitly drives and RTC-holds `SD_CS` (GPIO 7) HIGH across active and deep sleep states so an inserted MicroSD card never interferes with SX1262 LoRa SPI traffic or leaks current.
- **80 MHz Low-Power MCU Tuning & Peripheral Rails:** Reduced default CPU clock from 240 MHz to 80 MHz (~60% active current reduction), disabled audio codec/amp (`IO41`/`IO44`), and latched all peripheral power gating pins in deep sleep.
- **Non-Blocking Sensor & Battery State Machines:** Two-stage `millis()`-driven state machines for AHT20 sensor warmup (`IO40`) and battery ADC settling (`IO43`) with 12-point LiPo discharge curve.
- **$O(1)$ Circular Ring Buffers & Octal PSRAM Allocation:** 256-frame offline message queue allocated in 8 MB external PSRAM, saving ~44.5 KB of internal SRAM for BLE stack stability.
- **RF Safety Limit:** Enforced +22 dBm ceiling (`MAX_LORA_TX_POWER=22`) on CLI `set tx` command and RadioLib initialization.

---

## 🚀 1-Click Flashing on Windows

### Option A: Interactive PowerShell Script (`flash.ps1`)

Run in PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\flash.ps1
```

Or specify COM port and role directly:

```powershell
# Flash Companion BLE
powershell -ExecutionPolicy Bypass -File .\flash.ps1 -Port COM15 -Role ble

# Flash Repeater
powershell -ExecutionPolicy Bypass -File .\flash.ps1 -Port COM15 -Role repeater

# Flash KISS Modem
powershell -ExecutionPolicy Bypass -File .\flash.ps1 -Port COM15 -Role kiss
```

### Option B: Interactive Batch Script (`flash.bat`)

Double-click `flash.bat` or run from cmd:

```cmd
# Interactive role menu
flash.bat COM15

# Direct role argument (1=BLE, 2=USB, 3=Repeater, 4=Room, 5=KISS)
flash.bat COM15 3
```

---

## 🛠️ Manual Flashing via esptool.py

### Flash Any Unified Binary (Offset `0x0`)

```bash
# Companion Radio BLE (Default)
python -m esptool --port COM15 --baud 921600 --chip esp32s3 write_flash 0x0 MeshCore-NM-EPD-420-BW-companion_radio_ble-v1.18.0-mod-rev0.2.bin

# Companion Radio USB
python -m esptool --port COM15 --baud 921600 --chip esp32s3 write_flash 0x0 MeshCore-NM-EPD-420-BW-companion_radio_usb-v1.18.0-mod-rev0.2.bin

# Repeater
python -m esptool --port COM15 --baud 921600 --chip esp32s3 write_flash 0x0 MeshCore-NM-EPD-420-BW-repeater-v1.18.0-mod-rev0.2.bin

# Room Server
python -m esptool --port COM15 --baud 921600 --chip esp32s3 write_flash 0x0 MeshCore-NM-EPD-420-BW-room_server-v1.18.0-mod-rev0.2.bin

# KISS Modem
python -m esptool --port COM15 --baud 921600 --chip esp32s3 write_flash 0x0 MeshCore-NM-EPD-420-BW-kiss_modem-v1.18.0-mod-rev0.2.bin
```

### Post-Flash Reset (Crucial for ESP32-S3 USB-JTAG)

If the board remains in download mode after flashing, trigger a hardware watchdog reset:

```bash
python -m esptool --port COM15 --before no-reset --after watchdog-reset flash_id
```

---

## 🌐 Web Browser Flashing (ESP Web Tools)

You can flash any of the unified `.bin` files directly in Chrome or Edge via [ESP Web Tools](https://esphome.github.io/esp-web-tools/):

1. Connect RockBase NM-EPD-420-BW via USB-C.
2. Select your desired `.bin` file from the table above.
3. Flash to offset `0x0` at `921600` or `115200` baud.
