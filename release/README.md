# MeshCore-NM-EPD-420-BW v1.17.1-mod Firmware Release

This directory contains pre-compiled, hardware-verified binaries for the **RockBase NM-EPD-420-BW** board (ESP32-S3R8 + 4.2" E-Paper + SX1262 LoRa + AHT20).

---

## 1. File Descriptions

| File | Size | Flash Offset | Description |
|---|---|---|---|
| **`MeshCore-NM-EPD-420-BW-v1.17.1-mod.bin`** | **1.92 MB** | **`0x00000000`** | **Unified image:** Contains bootloader, partition table, boot_app0, and application firmware merged into a single image. **(Recommended)** |
| `bootloader.bin` | 18.8 KB | `0x0000` | ESP32-S3 ROM bootloader (QIO/OPI, 80MHz) |
| `partitions.bin` | 3.0 KB | `0x8000` | 16 MB custom partition map |
| `boot_app0.bin` | 8.0 KB | `0xe000` | ESP-IDF OTA data initialization partition |
| `firmware.bin` | 1.86 MB | `0x10000` | Application firmware |

---

## 2. Quick Flashing on Windows

### Option A: One-Click Batch Script (`flash.bat`)
Double-click `flash.bat` or run from command prompt:
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
python -m esptool --port COM15 --baud 921600 --chip esp32s3 write_flash 0x0 MeshCore-NM-EPD-420-BW-v1.17.1-mod.bin
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
