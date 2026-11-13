# RockBase NM-EPD-420 MOD — Release Notes

**Target Hardware:** RockBase NM-EPD-420-BW (ESP32-S3R8, 16 MB Flash, 8 MB Octal PSRAM, 4.2" 400x300 B/W E-Paper SSD1683, Semtech SX1262 LoRa, AHT20, microSD)  
**Primary Branches:** `main`, `nm-epd-420-bw-mod`  
**Maintainer:** [@v3n0m3n0](https://github.com/v3n0m3n0)

---

## [v1.18.0-mod-rev0.3] — Current Release

**Base:** Upstream MeshCore v1.18.0 (`81dbae5`, protocol version 14)  
**Roles Included:** `companion_radio_ble`, `companion_radio_usb`, `repeater`, `room_server`, `kiss_modem`  
**Compare:** [`v1.18.0-mod-rev0.2...v1.18.0-mod-rev0.3`](https://github.com/v3n0m3n0/MeshCore-NM-EPD-420-MOD/compare/v1.18.0-mod-rev0.2...v1.18.0-mod-rev0.3)

### Executive Summary

Release **v1.18.0-mod-rev0.3** hardens ESP32-S3 hardware stability, RadioLib concurrency, USB-Serial/JTAG host communication, and on-device RF observability across all 5 MeshCore firmware roles while shipping clean, host-path-anonymized unified `0x0` images.

### Key Changes

#### 1. Power Rails, Non-Blocking Sensors, Sleep & Clocking (`NMEPD420Board`, `NMEPD420EnvironmentSensorManager`, `NMEPD420Display`)
* **160 MHz CPU Frequency & Silent EPD Init:** Raised default CPU frequency from 80 MHz to 160 MHz in `boards/nm-epd-420-bw.json` and `variants/nm_epd_420_bw/platformio.ini` for deterministic BLE/LoRa/display concurrency headroom, and silenced `GxEPD2` serial init debug chatter (`display.init(0, true, 2, false)`).
* **Safe SX1262 Power-Up & Deep-Sleep ESD Back-Power Isolation:** Drives `SX126X_POWER_EN` (`IO47`) HIGH and waits for VDD/TCXO stabilization before asserting `P_LORA_NSS`/`PIN_SDCARD_CS` HIGH at boot; isolates SX1262 SPI pins (`SCLK`, `MOSI`, `RESET` -> `INPUT`, `NSS` latched `LOW`) before cutting `SX126X_POWER_EN` in `enterDeepSleep()` to prevent back-powering through SPI ESD diodes. Also sets defined idle states for display pins in headless builds (`KISS Modem`).
* **Interruptible WFI Idle Sleep & USB SOF Host Detection:** Replaced OPI-PSRAM `esp_light_sleep_start()` lockups on prebuilt `qio_opi` cores with interruptible 20 ms WFI slices (`sleep()`) that keep SX1262 RX active, feed TWDT (`esp_task_wdt_reset()`), and wake promptly on `DIO1` RX, `BOOT` button press, EPD busy state, or USB host attach. Implemented `isUsbPowered()` / `isUsbHostActive()` tracking `USB_SERIAL_JTAG.fram_num.sof_frame_index` with a 5 s holdoff window.
* **Non-Blocking Battery ADC (`BattAdcState`) & Calibration API:** Converted periodic battery sampling in `NMEPD420Board::loop()` to a non-blocking state machine (`ADC_IDLE` -> `ADC_SETTLING` -> `ADC_SAMPLING`) with cached boot seeding and implemented `setAdcMultiplier()` / `getAdcMultiplier()`.
* **Non-Blocking AHT20 State Machine:** Converted `NMEPD420EnvironmentSensorManager` background refreshes to a non-blocking 45 s I2C state machine (`WAIT_WARMUP`, `WAIT_INIT`, `WAIT_MEASURE`, `POLL_READY`) keeping `TEMP_CTL` (`IO40`) stably powered during active runtime.

#### 2. Thread-Safe RadioLib Driver, SX1262 Configuration & Telemetry Split
* **Critical-Section ISR Protection (`RadioLibWrappers`):** Protected `state` reads and writes shared with the `DIO1` `setFlag()` ISR using ESP32 `portMUX_TYPE` spinlocks (`portENTER_CRITICAL` / `portENTER_CRITICAL_ISR`), eliminating lost interrupt flags and idle/RX races across cores.
* **Safe `setParams()` Reconfiguration & `SX126X_REGISTER_PATCH=1`:** `CustomSX1262Wrapper::setParams()` cleanly exits RX (`idle()` -> `standby()`), applies radio parameters, restores `setRxBoostedGainMode()`, recalculates packet timing windows, and resumes `startRecv()` only if previously receiving. Enabled `SX126X_REGISTER_PATCH=1` and documented `set af 9` in `platformio.ini` for EU868 869.618 MHz 10% duty-cycle operation.
* **Separated CRC vs SPI Error Counters (`StatsFormatHelper`):** Split receive/control error accounting into `n_crc_errors` (`crc_err`: `RADIOLIB_ERR_CRC_MISMATCH` / `RADIOLIB_ERR_LORA_HEADER_DAMAGED`) and `n_spi_errors` (`spi_err`), exposed via SFINAE in `StatsFormatHelper::formatRadioStats()`.

#### 3. Cross-Role `RF HEALTH` UI, microSD Libpcap Logger & Host Interface Fixes
* **Shared `RF HEALTH` E-Paper View (`NMEPD420RfHealthUI`):** Added an `RF HEALTH` diagnostic screen across Companion Radio (`HomeScreen` carousel, with `Enter` triggering `sd_logger.retryMount()` + `forceFullRefresh()`), Repeater, and Room Server (`USER` toggles Main / RF Health; calmer 5 s partial refresh cadence, removed premature `AUTO_OFF_MILLIS` timeout). Displays frequency/SF, 3 dBm quantized noise floor, last RSSI/SNR, `RX / CRC / SPI` counters, TX/RX airtime, remaining duty budget (`af`), live `mV` / ADC multiplier / `USB|BAT` status, and microSD logger state.
* **Shared-HSPI microSD RF Logger (`NMEPD420SDLogger`):** Added an optional FAT32 microSD packet logger backed by a 32-entry non-blocking RAM ring buffer that arbitrates HSPI when the radio is idle (`!radio_driver.isInRecvMode()`), writing `/mesh_capture.pcap` (`DLT_USER0=147`) and `/mesh_rf.log`. Wired RX/TX capture and `sd status|retry|on|off|clear|pcap-test|ring-test` CLI commands on Repeater and Room Server, `RX_RAW` capture + `Enter` retry on Companion Radio, and RX/TX logging on KISS Modem.
* **Windows USB CDC Reliability (Repeater & Room Server):** Added a post-flash USB CDC re-latch wait (`15 ms` max) and trailing `\r\n` padding when CLI replies hit exact 64-byte bulk packet boundaries to avoid Windows `usbser.sys` ZLP stalls.
* **KISS Modem Radio Config Sync:** Synchronized `KissModem` startup radio parameters from build-time `LORA_FREQ`, `LORA_BW`, `LORA_SF`, `LORA_CR`, and `LORA_TX_POWER` macros.
* **Anonymized Release Packaging & Checksums:** Restored `-fmacro-prefix-map` / `-ffile-prefix-map` flags across all 5 environments, introduced parameterized [`build_and_release.ps1`](build_and_release.ps1) with host-path leak enforcement and `release/SHA256SUMS.txt` generation, and updated [`release/flash.ps1`](release/flash.ps1) / [`release/flash.bat`](release/flash.bat).

### Unified `0x0` Release Artifacts & SHA256 Checksums (`v1.18.0-mod-rev0.3`)

| Role | Binary | Size | SHA256 |
| --- | --- | :---: | --- |
| **Companion Radio BLE** | `MeshCore-NM-EPD-420-BW-companion_radio_ble-v1.18.0-mod-rev0.3.bin` *(default alias: `MeshCore-NM-EPD-420-BW-v1.18.0-mod-rev0.3.bin` / `firmware.bin`)* | 2 009 152 B | `31abc5d913198aac0093d56706ffbe6d9edf7d3639752ccfbebbd647e5c08b62` |
| **Companion Radio USB** | `MeshCore-NM-EPD-420-BW-companion_radio_usb-v1.18.0-mod-rev0.3.bin` | 1 459 760 B | `d96811cbd8b8fe5e812bb3079cb694b5f150f59c9429769e118d3452ba4b14b8` |
| **Repeater** | `MeshCore-NM-EPD-420-BW-repeater-v1.18.0-mod-rev0.3.bin` | 1 608 368 B | `ef5c2864219d999cb50af04a4469ddcab8c6d8e36aebd5a8b21ae5f5d94174c1` |
| **Room Server** | `MeshCore-NM-EPD-420-BW-room_server-v1.18.0-mod-rev0.3.bin` | 1 603 600 B | `963f9af36b4a87ccc6690ef4bbacf3a9b7e9ae5b3c773e647912a061b5e6107f` |
| **KISS Modem** | `MeshCore-NM-EPD-420-BW-kiss_modem-v1.18.0-mod-rev0.3.bin` | 1 350 784 B | `d9c75fdf57fc9db3083d06e56fe13fadd5b10b7bf2252c0d4886233f0357c4fa` |

---

## [v1.18.0-mod-rev0.2]

**Base:** Upstream MeshCore v1.18.0 (`81dbae5`, protocol version 14)  
**Roles Included:** `companion_radio_ble`, `companion_radio_usb`, `repeater`, `room_server`, `kiss_modem`  
**Compare:** [`v1.18.0-mod-rev0.1...v1.18.0-mod-rev0.2`](https://github.com/v3n0m3n0/MeshCore-NM-EPD-420-MOD/compare/v1.18.0-mod-rev0.1...v1.18.0-mod-rev0.2)

### Executive Summary

Release **v1.18.0-mod-rev0.2** delivers critical power-rail stability, LiPo discharge protection, I2C bus reliability, and deep-sleep GPIO hold fixes across all 5 firmware roles, alongside flasher tooling improvements on Windows.

### Key Changes

* **Deep-Sleep Digital GPIO Holds & LoRa Rail Shutdown:** Added `gpio_hold_en()` + `gpio_deep_sleep_hold_en()` across peripheral power gates (`TEMP_CTL` IO40, `PA_CTRL` IO41, `ADC_EN` IO43, `CODEC_EN` IO44, `SX126X_POWER_EN` IO47) so non-RTC pins do not float Hi-Z in Deep Sleep. Cuts power to SX1262 LoRa (`IO47` LOW), disables false `DIO1` wakeups when radio power is off, and RTC-holds `P_LORA_NSS` (`IO8`) and `PIN_SDCARD_CS` (`IO7`) HIGH.
* **Glitch-Free Wakeup Latch Release:** Configured `NMEPD420Board::begin()` to drive SPI CS and rail outputs into deterministic idle levels *before* releasing `gpio_deep_sleep_hold_dis()` / `gpio_hold_dis()`, eliminating SPI CS glitches on wake.
* **Safe I2C Startup Sequencing (Anti-ESD Clamping):** Enabled `CODEC_EN` (`IO44`) and `TEMP_CTL` (`IO40`) prior to `ESP32Board::begin()` I2C setup (`SDA` IO39 / `SCL` IO38), eliminating parasitic ESD diode clamping of the shared I2C bus to GND while keeping the speaker amplifier (`PA_CTRL` IO41) OFF.
* **LiPo Protection & Zero-Drain ADC Divider:** Configured `AUTO_SHUTDOWN_MILLIVOLTS=3300`, `BATT_MIN_MILLIVOLTS=3000`, and `BATT_MAX_MILLIVOLTS=4200`, and switched `PIN_ADC_CTRL` (`IO43`) OFF immediately after the 20 ms RC settling + 8-sample read in `readRawBattMilliVolts()`.
* **Native AHT20 I2C Driver (`0xBE` Calibration Command):** Replaced Adafruit AHTX0's `0xE1` init sequence with native `Wire` transactions at `0x38` sending the AHT20 calibration command `[0xBE, 0x08, 0x00]`, `[0xAC, 0x33, 0x00]` trigger, and busy-bit polling tied to cold power-cycles and UI forced refreshes.
* **Windows Flasher Fixes:** Improved `release/flash.ps1` with UTF-8 console encoding and `tool-esptoolpy` auto-fallback, and fixed batch variable expansion in `release/flash.bat`.

---

## [v1.18.0-mod-rev0.1]

**Base:** Upstream MeshCore v1.18.0 (`81dbae5`, protocol version 14)  
**Roles Included:** `companion_radio_ble`, `companion_radio_usb`, `repeater`, `room_server`, `kiss_modem`  
**Compare:** [`v1.17.1-mod-rev0.3...v1.18.0-mod-rev0.1`](https://github.com/v3n0m3n0/MeshCore-NM-EPD-420-MOD/compare/v1.17.1-mod-rev0.3...v1.18.0-mod-rev0.1)

### Executive Summary

Release **v1.18.0-mod-rev0.1** upgrades the NM-EPD-420-BW fork from MeshCore v1.17.1 to **MeshCore v1.18.0** (protocol v14) and introduces the complete **5-role firmware suite** (`companion_radio_ble`, `companion_radio_usb`, `repeater`, `room_server`, `kiss_modem`) while carrying forward every board-level E-Paper, PSRAM, sensor, and power optimization.

### Key Changes

* **Upstream v1.18.0 Codebase Merge:** Brought in protocol v14 features, including interactive Companion CLI over BLE/WiFi/Serial (`CMD_RUN_CLI_COMMAND`), decoupled UI event listeners (`MyMesh::Listener`), and STM32/portability build fixes (`<api/itoa.h>`).
* **Full 5-Role Suite Packaging:** Compiled and bundled unified `0x0` images for all 5 official roles sharing `NMEPD420Board`, `NMEPD420Display`, `NMEPD420EnvironmentSensorManager`, and `CommonRadioPrefs`.
* **Zero-Hop Repeater Discovery Screen (`HomePage::DISCOVERY`):** Integrated MeshCore v1.18.0 repeater/node discovery into the native 400x300 E-Paper layout (`row_step = 14`, UTF-8 transliteration, safe string bounds, and `discover: press Enter` prompt).
* **Dynamic WiFi & BLE Coexistence:** Enabled runtime WiFi station configuration (`ENABLE_WIFI_INTERFACE`), automatic reconnects, and IP/connection status display on the primary Home page alongside BLE.
* **Post-Merge Audit & Stability Fixes:** Guarded EPD mutex synchronization, fixed WiFi status display edge cases, ensured clean `BOOT` + `USER` double-click handling, and upgraded `flash.ps1` / `flash.bat` with interactive multi-role menus.
* **Preserved `-MOD` Hardware Foundation:** Retained asynchronous Core 0 E-Paper rendering, 256-frame Octal PSRAM offline queue, 16-frame circular BLE TX queue, shared HSPI `SD_CS` (`IO7`) isolation, AHT20 + 12-point LiPo ADC telemetry, and `+22 dBm` RF TX ceiling.

---

## [v1.17.1-mod-rev0.3] *(and Transitional v1.17.1-mod-rev0.2)*

**Base:** Upstream MeshCore v1.17.1 (`d9296435`)  
**Roles Included:** `companion_radio_ble` *(PlatformIO environments for USB/Repeater/Room/KISS restored in source)*  
**Compare:** [`v1.17.1-mod-rev0.1...v1.17.1-mod-rev0.3`](https://github.com/v3n0m3n0/MeshCore-NM-EPD-420-MOD/compare/v1.17.1-mod-rev0.1...v1.17.1-mod-rev0.3)

### Executive Summary

Release **v1.17.1-mod-rev0.3** transitions the v1.17.1 firmware to a non-blocking, asynchronous architecture by offloading E-Paper SPI waits to Core 0, allocating offline queues in 8 MB Octal PSRAM, eliminating blocking ADC/AHT20 delays, isolating the shared HSPI microSD bus, and restoring PlatformIO configurations for all 5 board roles. *(Note: internal milestone `v1.17.1-mod-rev0.2` served as a direct binary stepping-stone folded immediately into `v1.17.1-mod-rev0.3`, without a separate tag or source split.)*

### Key Changes (Delta over `v1.17.1-mod-rev0.1`)

* **Asynchronous Core 0 FreeRTOS E-Paper Task & 16 MHz FSPI:** Offloaded physical `SSD1683` updates (`display.display(...)` + `display.powerOff()`) to `epdRefreshTask` on Core 0 and increased FSPI clock from 4 MHz to 16 MHz, reducing main-loop UI frame commit latency from 600–2200 ms to `<1 ms`.
* **8 MB Octal PSRAM & $O(1)$ Circular Ring Buffers:** Dynamically allocated `MyMesh::offline_queue` (256 frames, freeing ~44.5 KB internal SRAM) in Octal PSRAM and converted `SerialBLEInterface::send_queue` (expanded 4x from 4 to 16 frames) to $O(1)$ ring buffers.
* **Non-Blocking Sensor & ADC Timing:** Removed runtime `delay(20)` and `delay(50)` calls from active UI refreshes via two-stage `millis()` state machines with 15 s AHT20 caching.
* **Shared HSPI Bus MicroSD Guard (`PIN_SDCARD_CS` `IO7`):** Explicitly drives and RTC-holds `GPIO 7` HIGH during active operation and deep sleep to prevent bus contention and leakage when a microSD card is inserted.
* **Enhanced Unread Pagination, UTF-8 & Reset Feedback:** Added `Unread: X/Y` counter, bidirectional `BOOT` (`KEY_NEXT`) / `USER` (`KEY_PREV`) preview navigation, $O(\log K)$ binary-search text ellipsizing, `0xC2` symbol transliteration (`°`, `±`, `²`, `³`, `µ`, `·`, NBSP), on-screen progress during 5s factory reset, and multi-role PlatformIO targets.

---

## [v1.17.1-mod-rev0.1]

**Base:** Upstream MeshCore v1.17.1 (`d9296435`)  
**Roles Included:** `companion_radio_ble`

### Executive Summary

Initial **RockBase NM-EPD-420 MOD** release establishing memory safety fixes, full 400x300 native E-Paper support, AHT20 environmental telemetry, non-linear LiPo battery estimation, hardware deep-sleep wakeups, quiescent rail gating, and responsive UI navigation.

### Key Changes

* **Safety & Reliability Hardening:** Fixed UTF-8 out-of-bounds read in `translateUTF8ToBlocks()`, fixed 5-second `USER` (`IO45`) BLE bond clearing by erasing NVS pairing state properly, replaced unsafe `esp_task_wdt_deinit()` with an adaptive 15 s TWDT (`esp_task_wdt_reconfigure()`), removed conflicting UART pins (`TX=43`/`RX=44`) in `pins_arduino.h`, and resolved simultaneous `BOOT`/`USER` button polling races.
* **Native `400x300` E-Paper Layout:** Unlocked the full `GYE042A87` (`SSD1683`) canvas (`200x150 @ 2.0x`), added hardware `CRC32` frame deduplication, `display.powerOff()` after refreshes, automatic full refresh every 30 partials, dynamic vertical centering (`NM_DY`), 8-row `RECENT` view, and streamlined carousel pages.
* **AHT20 & Non-Linear LiPo ADC Telemetry:** Integrated power-gated AHT20 temperature/humidity on Home and Radio screens (`600s` cadence + `Enter` manual trigger), fixed ESP32-S3 priming read order before `ADC_11db`, stabilized `ADC_EN` (`IO43`) with 8-sample averaging, and added a 12-point LiPo OCV percentage curve (`4.20V = 100%`, `3.82V = 50%`, `3.27V = 0%`).
* **Deep Sleep Wakeup & Message Preview:** Enabled `EXT0` (`BOOT IO0` LOW) and `EXT1` (`DIO1 IO14` HIGH) wakeups with Hibernate screen prompts, expanded Unread message buffer from 78 B to 161 B with safe UTF-8 boundary truncation, and eliminated the 1 Hz Unread E-Paper refresh storm via static `"now"` timestamps (`<60s`).

---

## Hardware Pinout Summary (Current)

For the complete hardware and pin mapping reference (FSPI E-Paper, shared HSPI SX1262 + `SD_CS IO7` + `LORA_EN IO47`, I2C `IO38/IO39`, power gates `IO40/IO41/IO43/IO44`, buttons & ADC), see [`README_NM_EPD_420.md`](README_NM_EPD_420.md#4-pin-mapping-reference).
