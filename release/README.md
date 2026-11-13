# RockBase NM-EPD-420 MOD — Release Package (`v1.18.0-mod-rev0.3`)

This directory contains pre-compiled, host-path-anonymized unified `0x0` binaries, Windows flash scripts, and `SHA256SUMS.txt` for the **RockBase NM-EPD-420-BW** board (`ESP32-S3R8`, `16 MB` Flash, `8 MB` Octal PSRAM, `4.2"` 400x300 E-Paper `SSD1683`, `SX1262` LoRa, `AHT20`, microSD).

> [!NOTE]
> Archive binaries from previous revisions (`v1.17.1-mod-*`, `v1.18.0-mod-rev0.1`, `v1.18.0-mod-rev0.2`) are hosted on the [GitHub Releases](https://github.com/v3n0m3n0/MeshCore-NM-EPD-420-MOD/releases) page so git stays compact and free of legacy un-anonymized binaries.

---

## Included Binaries (`v1.18.0-mod-rev0.3`)

| Role | Unified Binary (`0x0`) | Size | Primary Use Case | Active `-MOD` Highlights |
| --- | --- | :---: | --- | --- |
| **Companion Radio BLE** | [`MeshCore-NM-EPD-420-BW-companion_radio_ble-v1.18.0-mod-rev0.3.bin`](MeshCore-NM-EPD-420-BW-companion_radio_ble-v1.18.0-mod-rev0.3.bin)<br>*(also aliased as `MeshCore-NM-EPD-420-BW-v1.18.0-mod-rev0.3.bin` and `firmware.bin`)* | ~2.01 MB | Mobile companion (iOS & Android via BLE, WiFi station, CLI rescue) | Core 0 Async E-Paper (`400x300`), `RF HEALTH` view, microSD Libpcap RX logger, BLE + WiFi, 256-msg PSRAM queue, AHT20, LiPo ADC, 5s bond wipe |
| **Companion Radio USB** | [`MeshCore-NM-EPD-420-BW-companion_radio_usb-v1.18.0-mod-rev0.3.bin`](MeshCore-NM-EPD-420-BW-companion_radio_usb-v1.18.0-mod-rev0.3.bin) | ~1.46 MB | Desktop / Laptop companion via USB CDC serial terminal or web app | Core 0 Async E-Paper (`400x300`), `RF HEALTH` view, microSD Libpcap RX logger, USB CDC, 256-msg PSRAM queue, AHT20, LiPo ADC |
| **Repeater** | [`MeshCore-NM-EPD-420-BW-repeater-v1.18.0-mod-rev0.3.bin`](MeshCore-NM-EPD-420-BW-repeater-v1.18.0-mod-rev0.3.bin) | ~1.61 MB | Autonomous field / solar relay node extending mesh coverage | Async E-Paper status + `RF HEALTH`, microSD Libpcap logger (`sd *` CLI), 160 MHz + WFI idle sleep, USB CDC ZLP guard, TCXO SX1262 (`+22 dBm`) |
| **Room Server** | [`MeshCore-NM-EPD-420-BW-room_server-v1.18.0-mod-rev0.3.bin`](MeshCore-NM-EPD-420-BW-room_server-v1.18.0-mod-rev0.3.bin) | ~1.60 MB | Micro-BBS room server for shared channels, noticeboards & posts | Async E-Paper status + `RF HEALTH`, microSD Libpcap logger (`sd *` CLI), 160 MHz + WFI idle sleep, USB CDC ZLP guard, TCXO SX1262 (`+22 dBm`) |
| **KISS Modem** | [`MeshCore-NM-EPD-420-BW-kiss_modem-v1.18.0-mod-rev0.3.bin`](MeshCore-NM-EPD-420-BW-kiss_modem-v1.18.0-mod-rev0.3.bin) | ~1.35 MB | Serial KISS TNC bridge for AX.25, APRS, TCP/IP, or host tools | Headless radio bridge, microSD Libpcap RX/TX logging, synced build-macro radio defaults, 160 MHz MCU, shared HSPI microSD CS isolation |

Full release history, naming convention, and architectural details: [`RELEASE_NOTES.md`](../RELEASE_NOTES.md) · [`RELEASE.md`](../RELEASE.md#canonical-naming-standard) · [`README_NM_EPD_420.md`](../README_NM_EPD_420.md).

---

## Verify Checksums

Verify binary integrity against [`SHA256SUMS.txt`](SHA256SUMS.txt) before flashing:

```bash
# Linux / macOS
sha256sum -c SHA256SUMS.txt
```

```powershell
# Windows PowerShell (example for Companion BLE)
Get-FileHash .\MeshCore-NM-EPD-420-BW-companion_radio_ble-v1.18.0-mod-rev0.3.bin -Algorithm SHA256
```

---

## 1-Click Flashing on Windows

### Option A: PowerShell (`flash.ps1`)

```powershell
# Interactive COM port & role selector
powershell -ExecutionPolicy Bypass -File .\flash.ps1

# Direct port & role flags ('ble', 'usb', 'repeater', 'room_server', 'kiss_modem')
powershell -ExecutionPolicy Bypass -File .\flash.ps1 -Port COM15 -Role repeater
```

### Option B: Command Prompt (`flash.bat`)

Double-click `flash.bat` or pass port and role index (`1=BLE, 2=USB, 3=Repeater, 4=Room, 5=KISS`):

```cmd
flash.bat COM15
flash.bat COM15 3
```

---

## Manual Flashing via `esptool` (Offset `0x0`)

```bash
# Companion Radio BLE (Default)
python -m esptool --port COM15 --baud 921600 --chip esp32s3 --after watchdog-reset write_flash 0x0 MeshCore-NM-EPD-420-BW-companion_radio_ble-v1.18.0-mod-rev0.3.bin

# Companion Radio USB
python -m esptool --port COM15 --baud 921600 --chip esp32s3 --after watchdog-reset write_flash 0x0 MeshCore-NM-EPD-420-BW-companion_radio_usb-v1.18.0-mod-rev0.3.bin

# Repeater
python -m esptool --port COM15 --baud 921600 --chip esp32s3 --after watchdog-reset write_flash 0x0 MeshCore-NM-EPD-420-BW-repeater-v1.18.0-mod-rev0.3.bin

# Room Server
python -m esptool --port COM15 --baud 921600 --chip esp32s3 --after watchdog-reset write_flash 0x0 MeshCore-NM-EPD-420-BW-room_server-v1.18.0-mod-rev0.3.bin

# KISS Modem
python -m esptool --port COM15 --baud 921600 --chip esp32s3 --after watchdog-reset write_flash 0x0 MeshCore-NM-EPD-420-BW-kiss_modem-v1.18.0-mod-rev0.3.bin
```

### Post-Flash Reset (ESP32-S3 USB-Serial/JTAG)

If the board stays latched in download mode after flashing, force a hardware watchdog reset:

```bash
python -m esptool --port COM15 --before no-reset --after watchdog-reset flash_id
```

---

## Web Browser Flashing (Planned Roadmap)

*A hosted [ESP Web Tools](https://esphome.github.io/esp-web-tools/) manifest (`manifest.json` + GitHub Pages) for one-click Chrome/Edge installation across all 5 roles is planned on the [project roadmap](../README.md#development-roadmap-rockbase-nm-epd-420-mod).* Meanwhile, use [`flash.ps1`](flash.ps1), [`flash.bat`](flash.bat), or `esptool` at offset `0x0` above.
