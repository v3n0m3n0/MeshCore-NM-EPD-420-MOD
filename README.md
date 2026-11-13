# RockBase NM-EPD-420 MOD — MeshCore Multi-Role Firmware Suite

[![Build RockBase NM-EPD-420 MOD](https://github.com/v3n0m3n0/MeshCore-NM-EPD-420-MOD/actions/workflows/nm-epd-420-build.yml/badge.svg)](https://github.com/v3n0m3n0/MeshCore-NM-EPD-420-MOD/actions/workflows/nm-epd-420-build.yml)
[![Run Unit Tests](https://github.com/v3n0m3n0/MeshCore-NM-EPD-420-MOD/actions/workflows/run-unit-tests.yml/badge.svg)](https://github.com/v3n0m3n0/MeshCore-NM-EPD-420-MOD/actions/workflows/run-unit-tests.yml)
[![Latest Release](https://img.shields.io/github/v/release/v3n0m3n0/MeshCore-NM-EPD-420-MOD?include_prereleases&label=release)](https://github.com/v3n0m3n0/MeshCore-NM-EPD-420-MOD/releases)
[![Upstream Base](https://img.shields.io/badge/MeshCore%20Base-v1.18.0%20(proto%20v14)-blue)](https://github.com/meshcore-dev/MeshCore)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](license.txt)

> **RockBase NM-EPD-420 MOD (`v1.18.0-mod-rev0.3`)**  
> Hardware-verified, path-anonymized firmware modification of **MeshCore v1.18.0** for the **RockBase NM-EPD-420-BW** board (ESP32-S3R8, 16 MB Flash, 8 MB Octal PSRAM, 4.2" 400x300 B/W E-Paper SSD1683, Semtech SX1262 TCXO LoRa, AHT20, microSD), compiled into standalone single-file `0x0` images for **all 5 MeshCore roles**.

---

## Latest Release: [`v1.18.0-mod-rev0.3`](https://github.com/v3n0m3n0/MeshCore-NM-EPD-420-MOD/releases/tag/v1.18.0-mod-rev0.3)

**[Download `v1.18.0-mod-rev0.3` on GitHub Releases](https://github.com/v3n0m3n0/MeshCore-NM-EPD-420-MOD/releases/latest)** · **[Full Changelog (`RELEASE_NOTES.md`)](RELEASE_NOTES.md)** · **[Checksums (`SHA256SUMS.txt`)](release/SHA256SUMS.txt)** · **[Windows 1-Click Flashers](release/)**

Key improvements introduced in **`v1.18.0-mod-rev0.3`**:

- **Power Sequencing, Deep-Sleep ESD Isolation & WFI Sleep:** Ordered `SX126X_POWER_EN` (`IO47`) + Deep-Sleep SPI isolation (`SCLK`/`MOSI`/`RESET` -> `INPUT`, `NSS` latched `LOW`), interruptible `20 ms` WFI idle sleep replacing OPI-PSRAM light-sleep lockups, `USB_SERIAL_JTAG` SOF host detection (`isUsbPowered()`), `160 MHz` default CPU clock, and silent `GxEPD2` init.
- **Non-Blocking Battery ADC & AHT20 State Machines:** Asynchronous `BattAdcState` pipeline + live `setAdcMultiplier()` calibration and `45 s` `AHT20` background I2C state machine keeping `TEMP_CTL` (`IO40`) stably powered in active mode.
- **Thread-Safe RadioLib Driver & `crc_err` / `spi_err` Split:** ESP32 `portMUX_TYPE` spinlocks protecting `state` / `DIO1` ISR transitions, hardened `CustomSX1262Wrapper::setParams()` RX transitions (`SX126X_REGISTER_PATCH=1`), and separated LoRa CRC vs SPI fault counters (`StatsFormatHelper`).
- **Cross-Role `RF HEALTH` Screen & microSD Libpcap Logger:** Shared `RF HEALTH` E-Paper diagnostic view (`Companion`, `Repeater`, `Room Server`) paired with `NMEPD420SDLogger` capturing `/mesh_capture.pcap` (`DLT_USER0=147`) and `/mesh_rf.log`, `sd *` CLI commands (`Repeater`/`Room Server`), and `Enter` mount retry (`Companion`).
- **Host Interface & Build Hardening:** Repeater/Room Server USB CDC post-flash re-latch delay and 64-byte ZLP padding for Windows `usbser.sys`, `KissModem` startup radio macro synchronization, and verified `0`-leak prefix-mapped binaries across all 5 roles.

---

## Available Firmware Roles

All 5 roles share the unified board hardware abstraction (`NMEPD420Board`, `NMEPD420Display`, `NMEPD420EnvironmentSensorManager`, `NMEPD420SDLogger`, `NMEPD420RfHealthUI`, `CommonRadioPrefs`):

| Role | Unified Binary (`0x0`) | Size | Interfaces | Key Features |
| --- | --- | :---: | --- | --- |
| **Companion Radio BLE** | [`MeshCore-NM-EPD-420-BW-companion_radio_ble-v1.18.0-mod-rev0.3.bin`](release/MeshCore-NM-EPD-420-BW-companion_radio_ble-v1.18.0-mod-rev0.3.bin)<br>*(alias: [`MeshCore-NM-EPD-420-BW-v1.18.0-mod-rev0.3.bin`](release/MeshCore-NM-EPD-420-BW-v1.18.0-mod-rev0.3.bin))* | ~2.01 MB | BLE, WiFi, USB Serial | Mobile app connectivity, `CMD_RUN_CLI_COMMAND`, `RF HEALTH` E-Paper telemetry, optional FAT32 microSD Libpcap (`DLT_USER0=147`) RX logger, `HomePage::DISCOVERY`, Core 0 Async E-Paper, 256-msg Octal PSRAM queue, AHT20 + LiPo ADC, 5s `USER` bond wipe |
| **Companion Radio USB** | [`MeshCore-NM-EPD-420-BW-companion_radio_usb-v1.18.0-mod-rev0.3.bin`](release/MeshCore-NM-EPD-420-BW-companion_radio_usb-v1.18.0-mod-rev0.3.bin) | ~1.46 MB | USB CDC Serial | Desktop / Web client companion (`app.meshcore.nz` / Python / NodeJS), `RF HEALTH` E-Paper page, optional microSD Libpcap RX logger, Core 0 Async E-Paper, 256-msg PSRAM queue, AHT20 + LiPo ADC |
| **Repeater** | [`MeshCore-NM-EPD-420-BW-repeater-v1.18.0-mod-rev0.3.bin`](release/MeshCore-NM-EPD-420-BW-repeater-v1.18.0-mod-rev0.3.bin) | ~1.61 MB | LoRa, USB CDC CLI | Autonomous mesh relay (`MAX_NEIGHBOURS=50`), status + `RF HEALTH` E-Paper page, optional microSD Libpcap logger + `sd *` CLI, thread-safe RadioLib ISR state, 160 MHz + WFI idle sleep, USB CDC ZLP guard, TCXO SX1262 (+22 dBm) |
| **Room Server** | [`MeshCore-NM-EPD-420-BW-room_server-v1.18.0-mod-rev0.3.bin`](release/MeshCore-NM-EPD-420-BW-room_server-v1.18.0-mod-rev0.3.bin) | ~1.60 MB | LoRa, USB CDC CLI | Micro-BBS channel/noticeboard server, status + `RF HEALTH` E-Paper page, optional microSD Libpcap logger + `sd *` CLI, thread-safe RadioLib ISR state, 160 MHz + WFI idle sleep, USB CDC ZLP guard, TCXO SX1262 (+22 dBm) |
| **KISS Modem** | [`MeshCore-NM-EPD-420-BW-kiss_modem-v1.18.0-mod-rev0.3.bin`](release/MeshCore-NM-EPD-420-BW-kiss_modem-v1.18.0-mod-rev0.3.bin) | ~1.35 MB | USB KISS TNC | Headless serial KISS TNC bridge for AX.25, APRS, TCP/IP, or host software, build-macro radio defaults, optional microSD Libpcap RX/TX logging, 160 MHz MCU, shared HSPI microSD isolation |

> **Pre-built binaries, flash scripts & checksums:** [`release/`](release/) · [`SHA256SUMS.txt`](release/SHA256SUMS.txt)  
> **Hardware Deep Dive:** [`README_NM_EPD_420.md`](README_NM_EPD_420.md) · **Full Changelog:** [`RELEASE_NOTES.md`](RELEASE_NOTES.md)

---

## Hardware Highlights (`RockBase NM-EPD-420-BW`)

- **SoC & Memory:** ESP32-S3R8 (`160 MHz`, `qio_opi`), `16 MB` Flash (`DIO`), `8 MB` Octal PSRAM (hosts the 256-frame Companion offline queue).
- **Display Engine:** 4.2" `400x300` GYE042A87 (`SSD1683`) E-Paper on dedicated `16 MHz` FSPI (`IO1`/`IO2`/`IO46`/`IO4`/`IO5`/`IO6`), driven asynchronously by a Core 0 FreeRTOS task with `<1 ms` main-loop commit latency, hardware CRC32 frame caching, automatic full refresh every 30 partials, and post-refresh `powerOff()`.
- **Radio & Shared HSPI Bus:** Semtech `SX1262` (`1.8V` TCXO via DIO3, DIO2 RF switch, boosted RX gain, `+22 dBm` TX limit, `LORA_EN` power gate on `IO47`, register patch enabled) with portMUX-guarded ISR state and `SD_CS` (`IO7`) isolation + RTC hold.
- **Sensors & Power Gating:** Native I2C `AHT20` temperature/humidity (`TEMP_CTL` `IO40`), non-blocking 8-sample LiPo ADC (`ADC_EN` `IO43` + 12-point OCV curve + runtime multiplier calibration), `USB_SERIAL_JTAG` SOF host detection (`isUsbPowered()`), WFI idle slicing, and `EXT0` (`BOOT IO0`) / `EXT1` (`DIO1 IO14`) deep sleep wakeup.
- **Observability:** Cross-role `RF HEALTH` E-Paper telemetry page (`crc_err`, `spi_err`, quantized noise floor, RSSI/SNR, TX/RX airtime, remaining duty-cycle budget, mV/ADC multiplier, `USB|BAT`, microSD ring stats) plus zero-blocking FAT32 microSD Libpcap (`/mesh_capture.pcap`, `DLT_USER0=147`) and text (`/mesh_rf.log`) capture.

---

## Quick Start: Flashing Pre-Built Firmware

### Windows 1-Click Interactive Flashers

Download or clone this repository, connect the **RockBase NM-EPD-420-BW** over USB-C, and run either script inside [`release/`](release/):

```powershell
# PowerShell (interactive COM port & role selector)
cd release
powershell -ExecutionPolicy Bypass -File .\flash.ps1

# Direct role selection
powershell -ExecutionPolicy Bypass -File .\flash.ps1 -Port COM15 -Role repeater
```

Or double-click [`release/flash.bat`](release/flash.bat) / invoke from Command Prompt:

```cmd
cd release
flash.bat COM15 1
```

Roles for `flash.bat`: `1` Companion BLE (default), `2` Companion USB, `3` Repeater, `4` Room Server, `5` KISS Modem.

### Manual `esptool` Flash (`0x0` Unified Binary)

```bash
# Verify SHA256 checksums before flashing (Linux/macOS or PowerShell Get-FileHash)
cd release
sha256sum -c SHA256SUMS.txt

# Flash any role binary to offset 0x0 (replace COM15 with /dev/ttyACM0 on Linux)
python -m esptool --port COM15 --baud 921600 --chip esp32s3 --after watchdog-reset write_flash 0x0 MeshCore-NM-EPD-420-BW-companion_radio_ble-v1.18.0-mod-rev0.3.bin
```

> [!TIP]
> **ESP32-S3 USB-Serial/JTAG Reset Note:** If `esptool` leaves the MCU waiting in ROM download mode after flashing, force a hardware watchdog restart without power-cycling:
> ```bash
> python -m esptool --port COM15 --before no-reset --after watchdog-reset flash_id
> ```

---

## Controls, CLI & Regional Radio Configuration

### Physical Buttons (Companion / Repeater / Room Server)

- **`BOOT` (`IO0`):** Single-click cycles carousel pages / unread messages (`KEY_NEXT`). During the first 8s on the Splash screen, long-press enters CLI Rescue mode.
- **`USER` (`IO45`):** Single-click steps backward (`KEY_PREV`) / dismisses previews. Long-press triggers action/enter (`KEY_ENTER`, e.g., triggers immediate sensor read on `RADIO`, retries FAT32 microSD mount on `RF HEALTH`, sends Advert on `ADVERT`, triggers scan on `DISCOVERY`). Hold `USER` for **5 seconds during boot** (Companion BLE) to wipe BLE pairing bonds and reset config.
- **Double-Click (`BOOT` or `USER` on Companion UI):** Forces a full E-Paper clear/refresh cycle (`Screen Refresh`) and immediately refreshes sensor telemetry.

### microSD RF Logger CLI (`Repeater` & `Room Server`)

Insert a FAT32-formatted microSD card (captures `/mesh_capture.pcap` with `DLT_USER0=147` and `/mesh_rf.log`) and query or control capture via Serial/Admin CLI:

```text
sd status     -> mount state, card MB, ring occupancy (0..32), queued/written/dropped/errors
sd retry      -> retry card mount without rebooting
sd on         -> enable frame capture
sd off        -> disable frame capture
sd clear      -> clear RAM ring buffer and counters
sd pcap-test  -> validate Libpcap header & record serialization
sd ring-test  -> push synthetic RX/TX frames through the ring buffer
```

### EU868 10% Duty-Cycle Band (`869.618 MHz`)

Repeater/Companion radio airtime budget follows `duty_cycle = 1 / (1 + airtime_factor)`. For sub-band g3 (`869.618 MHz`, 10% duty cycle), configure via CLI after flashing:

```text
set af 9
```

*(Note on versioning: In-protocol `FIRMWARE_VERSION` in `MyMesh.h` stays `"v1.18.0"` to maintain strict compatibility with the 20-byte BLE info frame and SplashScreen parser across MeshCore mobile/web clients. Board revision `-mod-rev0.3` tracks the hardware/firmware package release.)*

---

## Building From Source (PlatformIO)

### Build All 5 Roles + Verify Anonymization & Checksums

```powershell
.\build_and_release.ps1 -Version v1.18.0-mod-rev0.3 -Clean
```

`build_and_release.ps1` compiles all 5 targets, generates unified `0x0` images (`mergebin`), enforces a zero host-path-leak scan across every generated `.bin` (protected via `-fmacro-prefix-map` / `-ffile-prefix-map` in [`variants/nm_epd_420_bw/platformio.ini`](variants/nm_epd_420_bw/platformio.ini)), and writes `release/SHA256SUMS.txt`.

### Build a Single Role

```bash
# Companion Radio BLE (Default)
pio run -e NM_EPD_420_BW_companion_radio_ble -t mergebin

# Companion Radio USB
pio run -e NM_EPD_420_BW_companion_radio_usb -t mergebin

# Repeater
pio run -e NM_EPD_420_BW_repeater -t mergebin

# Room Server
pio run -e NM_EPD_420_BW_room_server -t mergebin

# KISS Modem
pio run -e NM_EPD_420_BW_kiss_modem -t mergebin
```

Output unified images are placed at `.pio/build/<env>/firmware-merged.bin`.

### Unit Tests

```bash
pio test -e native -e native_kiss_modem -vv
```

---

## Documentation Index

| Document | Purpose |
| --- | --- |
| [`README_NM_EPD_420.md`](README_NM_EPD_420.md) | Complete board architecture, GPIO pinout map, E-Paper/PSRAM/power design notes, RF Health & microSD guide |
| [`RELEASE_NOTES.md`](RELEASE_NOTES.md) | Full chronological changelog across all `v1.17.1-mod-*` and `v1.18.0-mod-*` releases |
| [`release/README.md`](release/README.md) | Distribution package guide, checksum verification, Windows flasher scripts, manual `esptool` steps |
| [`RELEASE.md`](RELEASE.md) | Maintainer release checklist (local build, leak scan, checksums, release notes, tag & CI publish) |
| [`docs/`](docs/) | Upstream MeshCore protocol specifications ([CLI Commands](docs/cli_commands.md), [Companion Protocol](docs/companion_protocol.md), [KISS Protocol](docs/kiss_modem_protocol.md), [Packet Format](docs/packet_format.md), [FAQ](docs/faq.md)) |

---

## Development Roadmap (`RockBase NM-EPD-420 MOD`)

Planned improvements for upcoming releases (`rev0.4+` and future upstream syncs):

* **Hardware Audio Notification Synthesizer (`ES8311` + `NS4150B`):** Activate hardware-gated I2S chime/tone feedback via `CODEC_EN` (`IO44`) and `PA_CTRL` (`IO41`) for incoming messages and alerts while retaining zero quiescent drain in standby.
* **Filesystem Migration (`SPIFFS` -> `LittleFS`):** Migrate ESP32 internal flash storage from `SPIFFS` to `LittleFS` for better wear leveling, faster contact/channel serialization, and power-loss resilience.
* **Physical Peripherals & Soak Verification:** End-to-end physical soak testing of sustained FAT32 microSD Libpcap logging/rotation under heavy LoRa traffic, plus extended BLE + WiFi coexistence stability benchmarks on `Companion Radio BLE`.
* **Browser Web Flasher (ESP Web Tools):** Host a versioned `manifest.json` + GitHub Pages site so all 5 roles can be flashed with one click directly from Chrome/Edge.
* **Upstream MeshCore Tracking:** Periodic merges from [`meshcore-dev/MeshCore`](https://github.com/meshcore-dev/MeshCore) bringing protocol v2 / transport zoning improvements while preserving all `-MOD` board optimizations.

---

## Compatible MeshCore Clients

* **Companion Apps:** [Web Client](https://app.meshcore.nz) · [Android (Google Play)](https://play.google.com/store/apps/details?id=com.liamcottle.meshcore.android) · [iOS (App Store)](https://apps.apple.com/us/app/meshcore/id6742354151?platform=iphone) · [meshcore.js](https://github.com/liamcottle/meshcore.js) · [meshcore-cli (Python)](https://github.com/fdlamotte/meshcore-cli)
* **Repeater / Room Server Configuration:** [MeshCore Web Config](https://config.meshcore.io) (USB Serial) or LoRa Remote Management from the mobile app.

---

## About Upstream MeshCore, Support & License

This repository is a hardware-specific fork of [**MeshCore**](https://github.com/meshcore-dev/MeshCore) — a lightweight, portable C++ multi-hop LoRa packet routing system with role-based nodes (Companion, Repeater, Room Server, KISS Modem, Sensor).

* **RockBase NM-EPD-420-BW Hardware / MOD Issues & Releases:** Open an issue in [**v3n0m3n0/MeshCore-NM-EPD-420-MOD/issues**](https://github.com/v3n0m3n0/MeshCore-NM-EPD-420-MOD/issues).
* **Board Schematics & Vendor Reference:** [`RockBase-iot/NM-EPD-420`](https://github.com/RockBase-iot/NM-EPD-420) · [`RockBase-iot/meshcore`](https://github.com/RockBase-iot/meshcore/tree/nm-epd-420).
* **Upstream MeshCore Protocol & Apps:** [`meshcore-dev/MeshCore`](https://github.com/meshcore-dev/MeshCore) · [MeshCore Discord](https://meshcore.gg) · [MeshCore Docs](https://docs.meshcore.io).
* **Contributing:** See [`CONTRIBUTING.md`](CONTRIBUTING.md) and [`SECURITY.md`](SECURITY.md). Please target `main` or `nm-epd-420-bw-mod` when opening PRs specific to this fork.
* **License:** Released under the **MIT License** ([`license.txt`](license.txt)), consistent with upstream MeshCore.
