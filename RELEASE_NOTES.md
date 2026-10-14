# MeshCore-NM-EPD-420-BW v1.17.1-mod Release Notes

**Target Hardware:** RockBase NM-EPD-420-BW (ESP32-S3R8, 16MB Flash, 8MB Octal PSRAM, 4.2" 400×300 B/W E-Paper SSD1683, Semtech SX1262 LoRa, AHT20 sensor).  
**Branch:** `nm-epd-420-bw-mod`  
**Base:** MeshCore v1.17.1 (`d9296435`)  
**Maintainer:** [@v3n0m3n0](https://github.com/v3n0m3n0)

---

## Executive Summary

This release delivers an optimized, hardware-verified firmware modification package for the **RockBase NM-EPD-420-BW** companion radio board. It resolves memory safety vulnerabilities found in upstream code, unlocks the full 400×300 native e-paper panel, introduces an accurate non-linear LiPo battery measurement system, integrates onboard AHT20 environmental sensing, implements physical button deep-sleep wake, and eliminates quiescent power leakage across all peripherals.

Every package in this build has been thoroughly compiled with merge-bin and verified on physical hardware.

---

## Detailed Changelog & Features

### 1. Critical Audit Bugfixes (Package 1 - Commit `aae6b92`)
* **UTF-8 Out-of-Bounds Memory Read:** Fixed heap/stack corruption in `translateUTF8ToBlocks()` where truncated multi-byte UTF-8 characters (`0xE2` sequence) caused incrementing past the null-terminator.
* **BLE Bond Wipe on Power-Up:** Fixed a bug where holding the physical `USER` button (IO45) for 5 seconds failed to wipe BLE pairing bonds because `esp_ble_get_bond_device_num()` was called prior to BLE stack initialization. Now correctly clears the NVS pairing namespace.
* **Task Watchdog Timer (TWDT) Stability:** Replaced dangerous `esp_task_wdt_deinit()` with an adaptive 15-second TWDT configuration (`esp_task_wdt_reconfigure()`). Prevents false reboots during blocking ~2.2s e-paper refreshes while keeping background LoRa and BLE threads protected against deadlocks.
* **Pin Collision Cleanup:** Removed invalid default UART pin definitions (`TX=43, RX=44`) in `pins_arduino.h` that conflicted with `ADC_EN` (IO43) and `CODEC_EN` (IO44).
* **Button Race Condition Guard:** Resolved button state clobbering in `UITask::poll()` when `BOOT` and `USER` buttons were pressed in the same scheduler loop.

### 2. Native 400×300 E-Paper Display Engine (Feature F1 - Commit `d4345d6`)
* **Zero Margins / Full Canvas:** Expanded logical buffer from 128×128 (which left 72px black bars) to 200×150 @ 2.0x scale (400×300 native), utilizing 100% of the GYE042A87 e-paper panel.
* **Zero Power Leakage:** High-voltage charge pumps on the SSD1683 display controller are powered off (`display.powerOff()`) immediately after every refresh cycle.
* **CRC32 Frame Caching:** Computes hardware CRC32 of every render frame, skipping redundant e-paper updates when screen contents have not changed.
* **Partial Refresh Safety:** Configured fast hardware partial refreshes with an automatic full-window refresh every 30 cycles to prevent ghosting.

### 3. AHT20 Environmental Sensor Integration (Feature F4 - Commit `98cfb27`)
* Centered temperature and relative humidity readout (`%.1f C | %.0f%% RH`) on the primary Home screen.
* Power gating via `TEMP_CTL` (IO40) only during the brief ~50 ms I2C measurement, maintaining zero quiescent drain.

### 4. Advanced Battery ADC & LiPo OCV Curve (Feature F5 - Commit `cc036c2`)
* **ESP32-S3 ADC Order Bug Fix:** Eliminated `[E] __analogChannelConfig` error by adding a priming throwaway read before setting `ADC_11db` attenuation.
* **Settle Time:** Extended P-MOSFET divider stabilization delay to 20 ms (`BATT_ADC_SETTLE_MS = 20`) on IO43.
* **Noise Filtering:** 8-sample moving average with 1 ms inter-sample delay.
* **12-Point Non-Linear Curve:** Implemented piecewise linear interpolation based on the true LiPo open-circuit voltage discharge curve (`4.20V = 100%`, `3.82V = 50%`, `3.27V = 0%`).
* **Title Bar Display:** Live percentage numerical text (e.g., `100%`) rendered directly alongside the battery icon.

### 5. Deep Sleep & Hardware Button Wakeup (Feature F6 - Commit `d024279`)
* **Physical Button Wakeup:** Configured `esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BOOT_BTN, 0)` on `BOOT` (IO0, RTC GPIO 0, active LOW).
* **LoRa Packet Wakeup:** Configured `esp_sleep_enable_ext1_wakeup((1ULL << P_LORA_DIO_1), ESP_EXT1_WAKEUP_ANY_HIGH)` on LoRa `DIO1` (IO14, RTC GPIO 14).
* **Peripheral Isolation:** Shuts down power gates (IO40, IO43, IO41, IO44), latches LoRa NSS HIGH in RTC domain, and puts SSD1683 controller to sleep.
* **Hibernate Screen:** Displays `"Wake: Press BOOT"` and `"hibernating... Press BOOT to wake"` on `HomePage::SHUTDOWN`.

### 6. Full-Length Message Preview on "Unread" Page (Commit `8dd1b9c`)
* Expanded `MsgPreviewScreen::MsgEntry` text buffer from 78 bytes to 161 bytes (`MAX_TEXT_LEN + 1`).
* Full MeshCore packet payload (up to 160 characters) displays completely without truncation.
* Multi-byte UTF-8 sequence boundary protection prevents rendering garbled glyphs at truncated endings.
* Safe `snprintf` header formatting.

### 7. Carousel Navigation & Vertical Layout Centering (Commit `7a6908c` & `a01e25f`)
* Removed redundant empty `UI_SENSORS_PAGE` from carousel navigation for streamlined button cycling.
* Implemented dynamic geometric vertical centering (`NM_DY`) on all Home pages (FIRST, BLUETOOTH, ADVERT, SHUTDOWN, Splash), ensuring balanced spacing between the page dots and screen bottom.
* Expanded the `RECENT` heard node list to 8 rows with 14px row step, elegantly filling the full 4.2" display height.

### 8. Radio & Integrated Telemetry Page (Commit `8b3c72f`)
* Restored top-aligned layout (`y = 20`) for LoRa radio settings (`FQ`, `SF`, `BW`, `CR`, `TX`, `Noise floor`).
* Added horizontal separator line (`y = 67`).
* Integrated CayenneLPP onboard sensor telemetry with capitalized labels and colons:
  * `Voltage:` Live battery voltage with calculated LiPo percentage (`4.18V (100%)`).
  * `Temperature:` Ambient temperature (`%.1f C`).
  * `Humidity:` Relative humidity (`%.0f %% RH`).
* **10-Minute Sampling Interval (600s):** Sensors are queried once every 10 minutes, with immediate on-demand refresh triggered by pressing `Enter` on the Radio page.

### 9. UNREAD Screen Latency Elimination & Button Responsiveness
* **Elimination of 1 Hz Refresh Storm:** Fixed an issue where second-by-second timestamp increments (`%ds`) invalidated the display CRC every second on `MsgPreviewScreen`, causing the SSD1683 e-paper to execute non-stop, 100% blocking 1.5s refresh cycles that dropped button presses.
* **E-Paper Friendly Timestamp:** Messages under 60 seconds now display static `"now"` (applied to both UNREAD and RECENT screens), eliminating redundant screen redraws and keeping the CPU loop 100% responsive.
* **Extended Render Interval:** Configured e-paper render return interval to 60s (`AUTO_OFF_MILLIS=0`), waking immediately only when a new message arrives.
* **Unified Dual-Button Navigation:**
  * `BOOT` single-click (`KEY_NEXT`): Steps through unread messages; returns to Home on the last message.
  * `BOOT` double-click (`KEY_SELECT`) / long-press (`KEY_ENTER`): Instantly clears unread queue and returns to Home.
  * `USER` click (`KEY_PREV`): Instantly dismisses unread queue and returns to Home.

---

## Hardware Pinout Reference

```
EPD E-Paper (FSPI):
  SCK   -> IO2
  MOSI  -> IO1
  CS    -> IO46
  DC    -> IO4
  RST   -> IO5
  BUSY  -> IO6

SX1262 LoRa (HSPI):
  SCK   -> IO9
  MOSI  -> IO10
  MISO  -> IO11
  NSS   -> IO8
  RESET -> IO12
  BUSY  -> IO13
  DIO1  -> IO14 (RTC GPIO, Deep Sleep Wakeup)
  SD_CS -> IO7

I2C Bus (AHT20):
  SDA   -> IO39
  SCL   -> IO38

Power Gates (Active HIGH, 0V in standby):
  TEMP_CTL -> IO40  (AHT20 sensor VDD)
  PA_CTRL  -> IO41  (NS4150B audio amp)
  ADC_EN   -> IO43  (Battery voltage divider)
  CODEC_EN -> IO44  (ES8311 codec VDD)

Buttons & Battery:
  BOOT_BTN -> IO0   (Active LOW, RTC GPIO 0, Deep Sleep Wakeup)
  USER_BTN -> IO45  (Active LOW, Digital GPIO)
  BATT_ADC -> IO3   (ADC1_CH2, 2:1 resistive divider)
```

---

## Installation & Flashing

Pre-compiled distribution files are available in the [`release/`](release/) directory:

```bash
# Unified image at 0x0
python -m esptool --port COM15 --baud 921600 --chip esp32s3 write_flash 0x0 release/MeshCore-NM-EPD-420-BW-v1.17.1-mod.bin

# Clean software restart
python -m esptool --port COM15 --before no-reset --after watchdog-reset flash_id
```

Or double-click [`release/flash.bat`](release/flash.bat) on Windows.
