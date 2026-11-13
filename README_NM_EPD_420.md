# RockBase NM-EPD-420 MOD — Hardware & Architecture Reference

This document details the board-level engineering, GPIO mapping, power-sequencing rules, E-Paper rendering pipeline, RF telemetry, and build instructions for **RockBase NM-EPD-420 MOD** (targeting the **RockBase NM-EPD-420-BW** board on **MeshCore v1.18.0**).

---

## 1. Hardware Overview

| Subsystem | Component / Specification | Integration Notes |
| --- | --- | --- |
| **MCU** | ESP32-S3R8 (Dual-core Xtensa LX7 @ 160 MHz) | `qio_opi` memory mode, hardware USB-Serial/JTAG CDC |
| **Flash** | 16 MB QIO/DIO (`default_16MB.csv`) | Unified `0x0` images (`bootloader + partitions + boot_app0 + app`) |
| **PSRAM** | 8 MB Octal PSRAM (OPI) | Hosts the 256-frame Companion `offline_queue` |
| **Display** | 4.2" B/W E-Paper (`GYE042A87` / `SSD1683`, `400x300`) | Asynchronous Core 0 FreeRTOS task on `16 MHz` FSPI (`NMEPD420Display`) |
| **LoRa Radio** | Semtech `SX1262` (+22 dBm max, `1.8V` TCXO via DIO3) | Shared HSPI with `P_LORA_NSS=IO8`, `LORA_EN=IO47`, `DIO2` RF switch, register patch |
| **Storage** | microSD Card Slot (FAT32 on shared HSPI) | `SD_CS=IO7` RTC-held HIGH when idle/sleeping; zero-blocking `NMEPD420SDLogger` |
| **Env Sensor** | `AHT20` Temperature & Humidity (I2C `0x38`) | Native `0xBE` init, runtime `TEMP_CTL=IO40` rail gating |
| **Battery** | LiPo 1S via 2:1 resistive divider (`IO3`, `ADC_EN=IO43`) | Non-blocking 8-sample ADC pipeline + 12-point OCV curve |
| **Audio** *(HW ready)* | `ES8311` codec (`IO44`) + `NS4150B` PA (`IO41`) | Power-gated OFF by default; planned for upcoming I2S alerts |

---

## 2. Firmware Roles & Feature Matrix

All 5 PlatformIO environments inherit from `[NM_EPD_420_BW_base]` in [`variants/nm_epd_420_bw/platformio.ini`](variants/nm_epd_420_bw/platformio.ini):

| PlatformIO Environment | Role | UI Display | Comms Interface | microSD Logging |
| --- | --- | --- | --- | --- |
| `NM_EPD_420_BW_companion_radio_ble` | Companion Radio BLE *(default)* | 400x300 Carousel + Unread + Discovery + `RF HEALTH` | BLE (`123456`) + WiFi Station + USB Serial | `RX_RAW` + `Enter` retry on `RF HEALTH` |
| `NM_EPD_420_BW_companion_radio_usb` | Companion Radio USB | 400x300 Carousel + Unread + Discovery + `RF HEALTH` | USB CDC Serial (`ENABLE_USB_INTERFACE`) | `RX_RAW` + `Enter` retry on `RF HEALTH` |
| `NM_EPD_420_BW_repeater` | Repeater (`MAX_NEIGHBOURS=50`) | Status + `RF HEALTH` page | LoRa Relay + USB CDC / Remote CLI | `RX_RAW`, `TX_PKT`, `TX_FAIL` + `sd *` CLI |
| `NM_EPD_420_BW_room_server` | Room Server (Micro-BBS) | Status + `RF HEALTH` page | LoRa BBS + USB CDC / Remote CLI | `RX_RAW`, `TX_PKT`, `TX_FAIL` + `sd *` CLI |
| `NM_EPD_420_BW_kiss_modem` | KISS Modem (Serial TNC) | Headless (display & PA off) | USB KISS Serial Bridge | `RX_RAW`, `TX_PKT`, `TX_FAIL` |

---

## 3. Core Architecture & Hardware Fixes

### 3.1. Asynchronous Core 0 E-Paper Display Engine (`NMEPD420Display`)

1. **Full `400x300` Panel Utilization:** Uses `GxEPD2_420_GYE042A87` at logical `200x150` with `2.0x` scaling (`DISPLAY_ROTATION=0`), eliminating 72 px unused margins. Dynamic vertical centering (`NM_DY`) keeps Home screens balanced across the 4.2" canvas.
2. **Non-Blocking Core 0 Refresh Task (`epdRefreshTask`):** Main loop UI rendering writes into RAM and posts a frame notification in `<1 ms`, while FSPI transfers (`16 MHz`), BUSY polling (~600 ms partial / ~2.2 s full), and `display.powerOff()` charge-pump shutdown execute on Core 0 without blocking LoRa or BLE packet handling.
3. **Refresh Storm & Ghosting Protection:**
   * Hardware `CRC32` frame caching skips redundant physical updates when screen bytes are unchanged.
   * Full window refresh runs automatically every `30` partial cycles (or immediately on `BOOT`/`USER` double-click via `forceFullRefresh()`).
   * Messages under 60 seconds display static `"now"` on Unread and Recent views to avoid 1 Hz CRC invalidation storms.
4. **Polish / Extended UTF-8 Support:** Transliterates Polish diacritics (`ą ć ę ł ń ó ś ź ż`) and `0xC2`/`0xE2` punctuation (`°`, `±`, `²`, `³`, `µ`, `·`, NBSP, em-dash, quotes) in `DisplayDriver`, with `O(log K)` binary-search ellipsizing and strict multi-byte truncation safety.

### 3.2. Power Sequencing, Sleep Safety & Battery Telemetry (`NMEPD420Board`)

* **SX1262 Power Sequencing & Deep Sleep Pin Isolation:** Drives `SX126X_POWER_EN` (`IO47`) HIGH and waits for VDD + 1.8V TCXO stabilization before asserting SPI CS pins (`P_LORA_NSS=IO8`). Before entering Deep Sleep (`EXT0` on `BOOT IO0` LOW, `EXT1` on LoRa `DIO1 IO14` HIGH), isolates `SCLK`, `MOSI`, `RESET`, and `NSS` and RTC-holds `SD_CS` (`IO7`) HIGH prior to cutting `IO47` to eliminate parasitic ESD diode back-powering.
* **GPIO Hold & I2C ESD Clamp Prevention:** Unlatches digital pad holds at wake (`gpio_hold_dis()`), powers peripheral rails (`CODEC_EN=IO44`, `TEMP_CTL=IO40`) cleanly relative to I2C init (`SDA=IO39`, `SCL=IO38`), and holds power gates OFF in deep sleep.
* **Safe WFI Idle Sleep + USB SOF Detection:** Replaces OPI-PSRAM `esp_light_sleep_start()` lockups with interruptible `20 ms` WFI slices (`sleep()`) alongside `USB_SERIAL_JTAG` SOF frame-index tracking (`isUsbPowered()`, `5s` holdoff) and adaptive 15s Task Watchdog Timer (`esp_task_wdt_reconfigure()`).
* **LiPo ADC & Safety Thresholds:** Runs an ESP32-S3 priming throwaway read before `ADC_11db` attenuation, waits `20 ms` RC divider settling on `IO43` via non-blocking `BattAdcState`, averages 8 samples spaced `150 µs` apart, disables `IO43` immediately to eliminate divider drain, maps voltage via a 12-point non-linear OCV curve (`3000–4200 mV`, `AUTO_SHUTDOWN_MILLIVOLTS=3300`), and allows live calibration via `setAdcMultiplier()`.

### 3.3. RadioLib Concurrency & `RF HEALTH` Telemetry

* **Critical Sections:** `RadioLibWrapper` protects shared state transitions and `DIO1` ISR flags with ESP32 `portMUX_TYPE` spinlocks (`portENTER_CRITICAL(_ISR)`).
* **Safe Live Parameter Switching:** `CustomSX1262Wrapper::setParams()` transitions cleanly through `idle()`/`standby()`, preserves `SX126X_RX_BOOSTED_GAIN`, recalculates preamble/payload timers, and resumes RX only if active prior to switching.
* **Error Separation:** Splits `crc_err` (LoRa CRC/header mismatches) from `spi_err` (modem/SPI faults) in telemetry structs (`StatsFormatHelper`) and displays both on [`NMEPD420RfHealthUI.h`](variants/nm_epd_420_bw/NMEPD420RfHealthUI.h).
* **On-Device `RF HEALTH` Page:** Displays frequency/SF, 3 dBm quantized noise floor, last RSSI/SNR, `RX / CRC / SPI` packet counts, TX/RX airtime totals, remaining duty-cycle budget (`af`), live battery `mV` / ADC multiplier / `USB|BAT` source, and microSD logger status.

### 3.4. Shared-HSPI microSD Libpcap RF Logger (`NMEPD420SDLogger`)

* **Non-Blocking Ring Buffer:** Captures up to `32` frames (`RfDir`: `RX_RAW`, `RX_PKT`, `TX_PKT`, `TX_FAIL`, `264 B` snaplen) in RAM before arbitrating the shared HSPI bus (`P_LORA_NSS` `IO8` vs `PIN_SDCARD_CS` `IO7`) whenever the radio is idle (`!radio_driver.isInRecvMode()`), flushing in small batches (`2` records / `250 ms` interval or `8` threshold).
* **Capture Formats:** Appends binary Wireshark-compatible Libpcap packets to `/mesh_capture.pcap` (`PCAP_DLT_USER0 = 147`) and human-readable lines to `/mesh_rf.log`.
* **CLI (`Repeater` / `Room Server`):** `sd status`, `sd retry`, `sd on`, `sd off`, `sd clear`, `sd pcap-test`, `sd ring-test`. On Companion roles, pressing `USER` (`KEY_ENTER`) on the `RF HEALTH` page triggers `sd_logger.retryMount()`.

### 3.5. Memory, BLE Stability & Host Interface

* **Octal PSRAM Offline Queue:** Allocates the 256-frame `offline_queue` in 8 MB Octal PSRAM using an $O(1)$ circular ring buffer, keeping internal SRAM free for BLE + WiFi coexistence.
* **BLE Pairing Bond Wipe & Visual Reset Feedback:** Holding `USER` (`IO45`) for 5 seconds on Companion BLE startup clears the `esp_ble` bonding table and NVS pairing store with on-screen progress prompts. `SerialBLEInterface::send_queue` uses a 16-frame $O(1)$ circular buffer.
* **USB CDC Reliability:** Repeater and Room Server roles flush post-flash CDC re-latch delays (`flash_write`) and pad 64-byte multiples cleanly to prevent Windows USB-Serial/JTAG ZLP stalls during CLI transfers.
* **EU868 g3 Duty Cycle (`869.618 MHz`):** Configure `set af 9` via CLI (`duty_cycle = 1 / (1 + airtime_factor)`, bounding airtime budget to 10%).

---

## 4. Pin Mapping Reference

Verified against [`variants/nm_epd_420_bw/platformio.ini`](variants/nm_epd_420_bw/platformio.ini) and [`boards/nm-epd-420-bw.json`](boards/nm-epd-420-bw.json):

```text
EPD E-Paper (FSPI, 16 MHz):
  SCK      -> IO2
  MOSI     -> IO1
  MISO     -> -1 (not used)
  CS       -> IO46
  DC       -> IO4
  RST      -> IO5
  BUSY     -> IO6

SX1262 LoRa + microSD (Shared HSPI):
  SCK      -> IO9
  MOSI     -> IO10
  MISO     -> IO11
  NSS      -> IO8
  RESET    -> IO12
  BUSY     -> IO13
  DIO1     -> IO14 (RTC GPIO, EXT1 Deep Sleep Wakeup)
  LORA_EN  -> IO47 (SX1262 + TCXO power rail)
  SD_CS    -> IO7  (microSD CS, RTC-held HIGH when idle/sleeping)

I2C Bus (AHT20 + ES8311):
  SDA      -> IO39
  SCL      -> IO38

Power Gating (Active HIGH):
  TEMP_CTL -> IO40 (AHT20 VDD)
  PA_CTRL  -> IO41 (NS4150B audio amplifier)
  ADC_EN   -> IO43 (Battery voltage divider switch)
  CODEC_EN -> IO44 (ES8311 audio codec VDD)

Buttons & Battery:
  BOOT_BTN -> IO0  (Active LOW, RTC GPIO 0, EXT0 Deep Sleep Wakeup)
  USER_BTN -> IO45 (Active LOW, Digital GPIO)
  BATT_ADC -> IO3  (ADC1_CH2, 2:1 resistive divider)
```

---

## 5. Building & Flashing

### Full 5-Role Release Build (Recommended on Windows)

```powershell
.\build_and_release.ps1 -Version v1.18.0-mod-rev0.3 -Clean
```

Generates unified `0x0` `.bin` images in [`release/`](release/), checks all outputs for host filesystem path leaks (`-fmacro-prefix-map` / `-ffile-prefix-map`), and updates [`release/SHA256SUMS.txt`](release/SHA256SUMS.txt).

### Single-Target PlatformIO Build

```bash
# Replace env with any of the 5 NM_EPD_420_BW_* targets in variants/nm_epd_420_bw/platformio.ini
pio run -e NM_EPD_420_BW_companion_radio_ble -t mergebin
```

Flash output `.pio/build/<env>/firmware-merged.bin` at `0x0`:

```bash
python -m esptool --port COM15 --baud 921600 --chip esp32s3 --after watchdog-reset write_flash 0x0 .pio/build/NM_EPD_420_BW_companion_radio_ble/firmware-merged.bin
```

If USB-Serial/JTAG holds the ESP32-S3 in ROM bootloader mode after upload, issue:

```bash
python -m esptool --port COM15 --before no-reset --after watchdog-reset flash_id
```

---

## 6. Upstream References & Roadmap

* **Project Homepage & Roadmap:** [`README.md`](README.md#development-roadmap-rockbase-nm-epd-420-mod)
* **Full Changelog:** [`RELEASE_NOTES.md`](RELEASE_NOTES.md)
* **Upstream MeshCore:** [`meshcore-dev/MeshCore`](https://github.com/meshcore-dev/MeshCore)
* **Vendor Hardware Docs & Reference Fork:** [`RockBase-iot/NM-EPD-420`](https://github.com/RockBase-iot/NM-EPD-420) · [`RockBase-iot/meshcore`](https://github.com/RockBase-iot/meshcore/tree/nm-epd-420)
* **Maintainer:** [@v3n0m3n0](https://github.com/v3n0m3n0) · **License:** MIT ([`license.txt`](license.txt))
