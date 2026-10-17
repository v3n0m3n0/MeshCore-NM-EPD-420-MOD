# MeshCore-NM-EPD-420-BW v1.17.1-mod-rev0.3 Firmware Release

This directory contains pre-compiled, hardware-verified binaries for the **RockBase NM-EPD-420-BW** board (ESP32-S3R8 + 4.2" E-Paper + SX1262 LoRa + AHT20).

### Key Optimizations in rev0.3 (and rev0.2):
- **Asynchronous Core 0 FreeRTOS E-Paper Task & 16 MHz FSPI:** Offloaded blocking `GxEPD2` screen updates (~600 ms partial / ~2.2s full) to a dedicated FreeRTOS task on Core 0 and raised FSPI clock from 4 MHz to 16 MHz. Reduces UI frame commit latency on the main thread to <1 ms.
- **Two-Stage Non-Blocking Battery ADC & AHT20 Warmup State Machines:** Replaced synchronous `delay(20)` ADC settling and `delay(50)` AHT20 power-gate warmup delays with `millis()`-driven state machines.
- **Shared HSPI MicroSD Guard (`PIN_SDCARD_CS` IO7):** Explicitly drives and RTC-holds `SD_CS` (GPIO 7) HIGH so an inserted MicroSD card never interferes with SX1262 LoRa SPI traffic or leaks current in deep sleep.
- **$O(1)$ Circular Ring Buffers & Octal PSRAM Allocation:** Converted `MyMesh::offline_queue` (allocated in 8 MB Octal PSRAM, freeing ~44.5 KB of internal SRAM) and `SerialBLEInterface::send_queue` (expanded 4x from 4 to 16 frames) to $O(1)$ circular buffers.
- **$O(\log K)$ Binary-Search Text Ellipsizing & Extended UTF-8 (`0xC2`):** Replaced linear character stripping in `drawTextEllipsized()` with binary search and added `0xC2` symbol transliteration (`°`, `±`, `²`, `³`, `µ`, `·`, NBSP).
- **Bidirectional Unread Message Navigation & `X/Y` Pagination:** Added `Unread: X/Y` position counter on `MsgPreviewScreen`, mapped single-click `USER` (`KEY_PREV`) to step backward to newer messages, and added visual screen feedback during the 5-second `USER` button factory reset.
- **Low Power MCU Tuning & Deep Sleep Pin Latch:** Lowered default CPU clock from 240 MHz to 80 MHz (~60% reduction in MCU active current draw), added FreeRTOS idle yield, and latched all peripheral power gating pins (`IO40`, `IO43`, `IO41`, `IO44`) via `rtc_gpio_hold_en()` in deep sleep.
- **E-Paper Anti-Ghosting on Demand & Noise Floor Quantization:** Double-click on `USER` (IO45) triggers a full clean refresh cycle; quantized SX1262 noise floor on the RADIO page to 3 dBm steps.
- **High-Throughput BLE Notifications:** Reduced `BLE_WRITE_MIN_INTERVAL` from 60 ms to 20 ms for faster synchronization with the companion app.

### Future Optimization Backlog:
- **I2S Audio Notification Synthesizer (ES8311 + NS4150B):** Hardware-gated I2S tone generator (`CODEC_EN` IO44 + `PA_CTRL` IO41) for audible message alerts.
- **Filesystem Migration (`SPIFFS` -> `LittleFS`):** Transition ESP32 storage backend from `SPIFFS` to `LittleFS` for improved flash wear-leveling and power-loss resilience.

---

## 1. File Descriptions

| File | Size | Flash Offset | Description |
|---|---|---|---|
| **`MeshCore-NM-EPD-420-BW-v1.17.1-mod-rev0.3.bin`** | **1.92 MB** | **`0x00000000`** | **Unified image:** Contains bootloader, partition table, boot_app0, and application firmware merged into a single image. **(Recommended)** |
| `bootloader.bin` | 18.8 KB | `0x0000` | ESP32-S3 ROM bootloader (QIO/OPI, 80MHz) |
| `partitions.bin` | 3.0 KB | `0x8000` | 16 MB custom partition map |
| `boot_app0.bin` | 8.0 KB | `0xe000` | ESP-IDF OTA data initialization partition |
| `firmware.bin` | 1.86 MB | `0x10000` | Application firmware |

---

## 2. Quick Flashing on Windows

### Option A: One-Click Batch Script (`flash.bat`)
Double-click `flash.bat` (auto-detects the newest revision binary) or run from command prompt:
```cmd
flash.bat COM15
```

### Option B: PowerShell Script (`flash.ps1`)
```powershell
powershell -ExecutionPolicy Bypass -File .\flash.ps1
```

---

## 3. Manual Flashing via esptool.py

### Single Unified Image (Recommended):
```bash
python -m esptool --port COM15 --baud 921600 --chip esp32s3 write_flash 0x0 MeshCore-NM-EPD-420-BW-v1.17.1-mod-rev0.1.bin
```

### Separate Partitions:
```bash
python -m esptool --port COM15 --baud 921600 --chip esp32s3 write_flash \
    0x0000  bootloader.bin \
    0x8000  partitions.bin \
    0xe000  boot_app0.bin \
    0x10000 firmware.bin
```

### Post-Flash Reset (Crucial for ESP32-S3 USB-JTAG):
If the board remains in download mode after flashing, trigger a hardware watchdog reset:
```bash
python -m esptool --port COM15 --before no-reset --after watchdog-reset flash_id
```

---

## 4. Web Browser Flashing (ESP Web Tools / Chrome)

You can flash this device directly from Chrome/Edge using any web flasher (e.g., [ESP Web Tools](https://esphome.github.io/esp-web-tools/)):

- **File:** `MeshCore-NM-EPD-420-BW-v1.17.1-mod.bin`
- **Offset:** `0x0`
- **Baud Rate:** `921600` or `115200`
- **Chip:** `ESP32-S3`
