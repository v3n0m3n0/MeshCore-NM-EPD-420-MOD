# MeshCore-NM-EPD-420-BW v1.18.0-mod-rev0.2 Release Notes

**Target Hardware:** RockBase NM-EPD-420-BW (ESP32-S3R8, 16MB Flash, 8MB Octal PSRAM, 4.2" 400×300 B/W E-Paper SSD1683, Semtech SX1262 LoRa, AHT20 sensor).  
**Branch:** `nm-epd-420-bw-mod`  
**Base:** MeshCore Companion Firmware v1.18.0 (`81dbae5`, protocol version 14)  
**Maintainer:** [@v3n0m3n0](https://github.com/v3n0m3n0)

---

## Podsumowanie wydania (v1.18.0-mod-rev0.2) / Executive Summary

Wydanie **rev0.2** dla platformy **RockBase NM-EPD-420-BW** stanowi krytyczną aktualizację stabilności zasilania, ochrony baterii LiPo oraz niezawodności magistrali I2C i peryferiów pokładowych dla wszystkich 5 oficjalnych ról firmware'u MeshCore v1.18.0 (`companion_radio_ble`, `companion_radio_usb`, `repeater`, `room_server`, `kiss_modem`).

Wprowadzone poprawki eliminują ryzyko rozładowania baterii poniżej bezpiecznego progu chemicznego, zjawisko pasożytniczego clamping-u magistrali I2C (ESD protection clamping) oraz prądy upływu w trybie głębokiego uśpienia (deep sleep).

---

## Szczegółowy wykaz wprowadzonych poprawek (rev0.2)

### 1. Zatrzaski stanów GPIO w Deep Sleep i odcięcie zasilania LoRa (`SX126X_POWER_EN`)

* **Cyfrowe zatrzaski GPIO (`gpio_deep_sleep_hold_en()`):** ESP32-S3 domyślnie po wejściu w `esp_deep_sleep_start()` przestawia piny GPIO spoza domeny RTC w stan wysokiej impedancji (floating / Hi-Z). Wprowadzono wywołanie `gpio_hold_en()` dla wszystkich kluczowych linii sterujących zasilaniem peryferiów:
  * `AHTX0_POWER_PIN` (GPIO 40) – odcięcie zasilania sensora AHT20,
  * `PIN_AMP_ENABLE` (GPIO 41) – wyciszenie wzmacniacza audio NS4150B,
  * `PIN_ADC_CTRL` (GPIO 43) – rozłączenie klucza dzielnika napięcia baterii,
  * `PIN_ES8311_POWER` (GPIO 44) – odcięcie zasilania kodeka audio,
  * `SX126X_POWER_EN` (GPIO 47) – całkowite odcięcie zasilania modułu radiowego SX1262.
* **Aktywacja globalnego zatrzasku cyfrowego:** Przed uśpieniem wywoływane jest `gpio_deep_sleep_hold_en()`, co trwale blokuje stany wyjść cyfrowych na czas snu układu, redukując prąd uśpienia do minimum i eliminując prądy upływu.
* **Bezpieczne, bezglitchowe zwalnianie zatrzasków po wybudzeniu:** W procedurze `NMEPD420Board::begin()` wszystkie linie wyjściowe (w tym `P_LORA_NSS`, `PIN_SDCARD_CS`, szyny zasilające) są wymuszane w zdefiniowane stany aktywne (HIGH/LOW) **przed** wywołaniem `gpio_deep_sleep_hold_dis()` / `gpio_hold_dis()`. Zapobiega to przejściowym stanom nieustalonym (glitch) i kolizjom na magistrali SPI. Zwalnianie zatrzasków następuje bezwarunkowo przy każdym resecie/starcie MCU. Ponadto wybudzenie z pakietu LoRa na DIO1 jest blokowane w uśpieniu, gdy moduł radiowy SX1262 jest odcięty od zasilania (`SX126X_POWER_EN` LOW), co eliminuje fałszywe pętle wybudzeń.
* **Separacja magistrali LoRa i MicroSD:** Linie `P_LORA_NSS` (GPIO 8) oraz `PIN_SDCARD_CS` (GPIO 7) są trwale zatrzaskiwane w stanie wysokim w domenie RTC (`rtc_gpio_hold_en`), uniemożliwiając zakłócenie magistrali SPI przez kartę pamięci MicroSD.

### 2. Bezpieczna sekwencja zasilania magistrali I2C przed `ESP32Board::begin()` (Anty-ESD Clamping)

* **Zjawisko pasożytniczego clamping-u:** Magistrala I2C (SDA=GPIO 39, SCL=GPIO 38) jest współdzielona pomiędzy kodekiem audio ES8311 a sensorem AHT20. Pozostawienie szyn zasilających te układy w stanie odciętym (0V) przy jednoczesnym podciągnięciu linii SDA/SCL do 3.3V powoduje przewodzenie wewnętrznych diod zabezpieczających ESD tych układów do masy (parasitic ESD diode clamping), co ściąga napięcie na magistrali do ~0.6V i uniemożliwia komunikację I2C (błędy timeoutu).
* **Sekwencja rozruchowa:** W `NMEPD420Board::begin()` zasilanie układu kodeka (`PIN_ES8311_POWER` GPIO 44) oraz sensora (`AHTX0_POWER_PIN` GPIO 40) jest załączane do stanu wysokiego (3.3V) **przed** wywołaniem inicjalizacji bazowej `ESP32Board::begin()`. Wzmacniacz audio (`PIN_AMP_ENABLE` GPIO 41) pozostaje wyłączony (LOW), co zapewnia zerowy pobór prądu przez tor audio przy zachowaniu pełnej sprawności magistrali I2C.

### 3. Ochrona akumulatora LiPo i eliminacja drenowania przez klucz pomiarowy ADC

* **Trzypunktowe progi napięciowe LiPo:** W konfiguracji platformy (`variants/nm_epd_420_bw/platformio.ini`) dodano ścisłe limity napięć:
  * `AUTO_SHUTDOWN_MILLIVOLTS=3300` – automatyczne bezpieczne wyłączenie węzła / uśpienie przy spadku napięcia poniżej 3.30V (chroni ogniwo LiPo przed głębokim rozładowaniem i degradacją chemiczną poniżej dopuszczalnego progu 3.00V),
  * `BATT_MIN_MILLIVOLTS=3000` – bezwzględne minimum napięcia odcięcia (0%),
  * `BATT_MAX_MILLIVOLTS=4200` – nominalne pełne naładowanie (100%).
* **Symetryczne wyłączanie klucza pomiarowego `PIN_ADC_CTRL` (GPIO 43):**
  * Klucz tranzystorowy dzielnika napięcia baterii (P-MOSFET / N-MOSFET na GPIO 43) jest załączany (`digitalWrite(PIN_ADC_CTRL, HIGH)`) wyłącznie na czas 20 ms stabilizacji pojemności filtracyjnych (`BATT_SETTLE_MS = 20`) oraz 8-próbkowego uśrednianego pomiaru ADC.
  * Bezpośrednio po zakończeniu próbkowania w `NMEPD420Board::readRawBattMilliVolts()`, klucz jest natychmiast rozłączany (`digitalWrite(PIN_ADC_CTRL, LOW)`).
  * Eliminuje to ciągły upływ prądu przez dzielnik rezystancyjny 2:1 do masy, oszczędzając cenną energię akumulatora podczas pracy aktywnej oraz w uśpieniu.

### 4. Natywna obsługa sensora AHT20 protokołem I2C (komenda 0xBE)

* **Wyeliminowanie problemu biblioteki Adafruit AHTX0:** Standardowa biblioteka wysyła komendę kalibracji AHT10 (`0xE1, 0x08, 0x00`), podczas gdy sensor AHT20 bezwzględnie wymaga komendy `0xBE, 0x08, 0x00`. Skutkowało to brakiem ustawienia bitu kalibracji (bit 3 w rejestrze stanu 0x71) i sporadycznymi błędami inicjalizacji sensora po włączeniu zasilania.
* **Bezpośrednia implementacja Wire:** W `NMEPD420EnvironmentSensorManager` zaimplementowano bezpośrednią obsługę protokołu I2C:
  1. Sondowanie adresu I2C `0x38`.
  2. Odczyt rejestru statusu (komenda `0x71`).
  3. Jeśli bit kalibracji nie jest ustawiony (`(status & 0x08) == 0`), wysłanie właściwej sekwencji inicjalizacyjnej AHT20: `[0xBE, 0x08, 0x00]` i odczekanie 15 ms.
  4. Wyzwolenie pomiaru komendą `[0xAC, 0x33, 0x00]` z 80 ms czasem konwersji.
  5. 6-bajtowy odczyt z weryfikacją bitu zajętości (`status & 0x80 == 0`).

---

# MeshCore-NM-EPD-420-BW v1.18.0-mod-rev0.1 Release Notes

**Target Hardware:** RockBase NM-EPD-420-BW (ESP32-S3R8, 16MB Flash, 8MB Octal PSRAM, 4.2" 400×300 B/W E-Paper SSD1683, Semtech SX1262 LoRa, AHT20 sensor).  
**Branch:** `nm-epd-420-bw-mod`  
**Base:** MeshCore Companion Firmware v1.18.0 (`81dbae5`, protocol version 14)  
**Maintainer:** [@v3n0m3n0](https://github.com/v3n0m3n0)

---

## Executive Summary (v1.18.0-mod-rev0.1)

This release upgrades the custom RockBase NM-EPD-420-BW firmware from MeshCore v1.17.1 to the newly released upstream **MeshCore Companion Firmware v1.18.0**. It incorporates all upstream v1.18.0 protocol changes, interactive CLI command handling, zero-hop node discovery, abstract UI decoupling via `MyMesh::Listener`, and dynamic WiFi support, while fully preserving and verifying every single hardware optimization engineered for the NM-EPD-420 board.

In addition to Companion BLE, this release compiles and packages the complete suite of **all 5 official MeshCore firmware roles** into pre-built, unified single-file `0x0` flashable binaries: **Companion Radio BLE**, **Companion Radio USB**, **Repeater**, **Room Server**, and **KISS Modem**.

### Highlights

- **Full 5-Role Firmware Suite:** Standalone unified `0x0` images for all 5 roles (`companion_radio_ble`, `companion_radio_usb`, `repeater`, `room_server`, `kiss_modem`), each sharing the board-level `-MOD` hardware foundation (80 MHz CPU, 15s TWDT, MicroSD CS `IO7` bus isolation, AHT20/battery non-blocking state machines, and SX1262 22 dBm ceiling).
* **Upstream v1.18.0 Integration:** Full protocol v14 support including `CMD_RUN_CLI_COMMAND` (CLI over BLE/WiFi/Serial), zero-hop Node Discover UI (`HomePage::DISCOVERY`), and decoupled event listening via `MyMesh::Listener`.
* **Preserved Core 0 Asynchronous E-Paper Refresh:** 16 MHz FSPI refresh task maintains sub-millisecond frame dispatch with zero CPU blocking on the main thread across all UI-enabled roles.
* **Dynamic WiFi & BLE Support:** Dynamic WiFi interface with auto-reconnect and IP display, compatible with BLE coexistence on ESP32-S3.
* **8 MB Octal PSRAM Offline Queue:** Large 256-frame offline message buffer in high-speed PSRAM with $O(1)$ ring buffer management.
* **Interactive Multi-Role Flasher:** Upgraded `flash.ps1` and `flash.bat` with interactive menus allowing one-click flashing of any of the 5 roles.
* **Hardware Button & Power Management:** 5-second `USER` button factory reset, physical `BOOT` ext0 deep sleep wakeup, and complete peripheral rail isolation.

---

# MeshCore-NM-EPD-420-BW v1.17.1-mod-rev0.3 Release Notes

**Target Hardware:** RockBase NM-EPD-420-BW (ESP32-S3R8, 16MB Flash, 8MB Octal PSRAM, 4.2" 400×300 B/W E-Paper SSD1683, Semtech SX1262 LoRa, AHT20 sensor).  
**Branch:** `nm-epd-420-bw-mod`  
**Base:** MeshCore v1.17.1 (`d9296435`)  
**Maintainer:** [@v3n0m3n0](https://github.com/v3n0m3n0)

---

## Executive Summary

This release delivers an optimized, hardware-verified firmware modification package for the **RockBase NM-EPD-420-BW** companion radio board. It resolves memory safety vulnerabilities found in upstream code, unlocks the full 400×300 native e-paper panel, offloads e-paper refreshes to an asynchronous Core 0 FreeRTOS task over 16 MHz FSPI, introduces non-blocking state machines for battery ADC and AHT20 sensing, allocates offline message queues in 8 MB Octal PSRAM with $O(1)$ circular buffers, implements physical button deep-sleep wake, and eliminates quiescent power leakage across all peripherals.

Every package in this build has been thoroughly compiled with merge-bin and verified on physical hardware.

---

## Detailed Changelog & Features

### 1. Critical Audit Bugfixes

* **UTF-8 Out-of-Bounds Memory Read:** Fixed heap/stack corruption in `translateUTF8ToBlocks()` where truncated multi-byte UTF-8 characters (`0xE2` sequence) caused incrementing past the null-terminator.
* **BLE Bond Wipe on Power-Up:** Fixed a bug where holding the physical `USER` button (IO45) for 5 seconds failed to wipe BLE pairing bonds because `esp_ble_get_bond_device_num()` was called prior to BLE stack initialization. Now correctly clears the NVS pairing namespace.
* **Task Watchdog Timer (TWDT) Stability:** Replaced dangerous `esp_task_wdt_deinit()` with an adaptive 15-second TWDT configuration (`esp_task_wdt_reconfigure()`). Prevents false reboots during blocking ~2.2s e-paper refreshes while keeping background LoRa and BLE threads protected against deadlocks.
* **Pin Collision Cleanup:** Removed invalid default UART pin definitions (`TX=43, RX=44`) in `pins_arduino.h` that conflicted with `ADC_EN` (IO43) and `CODEC_EN` (IO44).
* **Button Race Condition Guard:** Resolved button state clobbering in `UITask::poll()` when `BOOT` and `USER` buttons were pressed in the same scheduler loop.

### 2. Native 400×300 E-Paper Display Engine

* **Zero Margins / Full Canvas:** Expanded logical buffer from 128×128 (which left 72px black bars) to 200×150 @ 2.0x scale (400×300 native), utilizing 100% of the GYE042A87 e-paper panel.
* **Zero Power Leakage:** High-voltage charge pumps on the SSD1683 display controller are powered off (`display.powerOff()`) immediately after every refresh cycle.
* **CRC32 Frame Caching:** Computes hardware CRC32 of every render frame, skipping redundant e-paper updates when screen contents have not changed.
* **Partial Refresh Safety:** Configured fast hardware partial refreshes with an automatic full-window refresh every 30 cycles to prevent ghosting.

### 3. AHT20 Environmental Sensor Integration

* Centered temperature and relative humidity readout (`%.1f C | %.0f%% RH`) on the primary Home screen.
* Power gating via `TEMP_CTL` (IO40) only during the brief ~50 ms I2C measurement, maintaining zero quiescent drain.

### 4. Advanced Battery ADC & LiPo OCV Curve

* **ESP32-S3 ADC Order Bug Fix:** Eliminated `[E] __analogChannelConfig` error by adding a priming throwaway read before configuring `ADC_11db` attenuation.
* **Settle Time:** Extended P-MOSFET divider stabilization delay to 20 ms (`BATT_ADC_SETTLE_MS = 20`) on IO43.
* **Noise Filtering:** 8-sample moving average with 150 µs inter-sample delay.
* **12-Point Non-Linear Curve:** Implemented piecewise linear interpolation based on the true LiPo open-circuit voltage discharge curve (`4.20V = 100%`, `3.82V = 50%`, `3.27V = 0%`).
* **Title Bar Display:** Live percentage numerical text (e.g., `100%`) rendered directly alongside the battery icon.

### 5. Deep Sleep & Hardware Button Wakeup

* **Physical Button Wakeup:** Configured `esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BOOT_BTN, 0)` on `BOOT` (IO0, RTC GPIO 0, active LOW).
* **LoRa Packet Wakeup:** Configured `esp_sleep_enable_ext1_wakeup((1ULL << P_LORA_DIO_1), ESP_EXT1_WAKEUP_ANY_HIGH)` on LoRa `DIO1` (IO14, RTC GPIO 14).
* **Peripheral Isolation:** Shuts down power gates (IO40, IO43, IO41, IO44), latches LoRa NSS (IO8) and shared MicroSD CS (IO7) HIGH in RTC domain, and puts SSD1683 controller to sleep.
* **Hibernate Screen:** Displays `"Wake: Press BOOT"` and `"hibernating... Press BOOT to wake"` on `HomePage::SHUTDOWN`.

### 6. Full-Length Message Preview on "Unread" Page

* Expanded `MsgPreviewScreen::MsgEntry` text buffer from 78 bytes to 161 bytes (`MAX_TEXT_LEN + 1`).
* Full MeshCore packet payload (up to 160 characters) displays completely without truncation.
* Multi-byte UTF-8 sequence boundary protection prevents rendering garbled glyphs at truncated endings.
* Safe `snprintf` header formatting.

### 7. Carousel Navigation & Vertical Layout Centering

* Removed redundant empty `UI_SENSORS_PAGE` from carousel navigation for streamlined button cycling.
* Implemented dynamic geometric vertical centering (`NM_DY`) on all Home pages (FIRST, BLUETOOTH, ADVERT, SHUTDOWN, Splash), ensuring balanced spacing between the page dots and screen bottom.
* Expanded the `RECENT` heard node list to 8 rows with 14px row step, elegantly filling the full 4.2" display height.

### 8. Radio & Integrated Telemetry Page

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
* **Bidirectional Dual-Button Navigation & `X/Y` Pagination:**
  * Header displays position indicator `Unread: X/Y` when multiple unread messages are queued.
  * `BOOT` single-click (`KEY_NEXT`): Advances to older unread message; returns to Home after the last message.
  * `USER` single-click (`KEY_PREV`): Steps back to newer unread message; dismisses preview when at the first message.
  * `BOOT` double-click (`KEY_SELECT`) / long-press (`KEY_ENTER`): Instantly clears unread queue and returns to Home.

### 10. Architecture & Concurrency Optimizations (rev0.3)

* **Asynchronous Core 0 FreeRTOS E-Paper Task & 16 MHz FSPI:** Offloaded physical SSD1683 refresh cycles (`display.display(true/false)` + `display.powerOff()`) to a background FreeRTOS task on Core 0 (`epdRefreshTask`) and raised FSPI clock from 4 MHz to 16 MHz. Reduces UI frame commit latency on the main thread from 600–2200 ms to <1 ms.
* **Non-Blocking State Machines for Battery ADC & AHT20 Warmup:** Eliminated synchronous `delay(20)` and `delay(50)` calls during runtime telemetry refreshes via two-stage `millis()` state machines.
* **Shared HSPI Bus MicroSD Protection (`PIN_SDCARD_CS` IO7):** Explicitly drives and RTC-holds GPIO 7 (`SD_CS`) HIGH so an inserted MicroSD card never interferes with SX1262 LoRa SPI transactions or leaks current in deep sleep.
* **$O(1)$ Circular Ring Buffers & Octal PSRAM Allocation:** Converted `MyMesh::offline_queue` (allocated in 8 MB Octal PSRAM, freeing ~44.5 KB of internal SRAM) and `SerialBLEInterface::send_queue` (expanded 4x from 4 to 16 frames) to $O(1)$ circular buffers.
* **$O(\log K)$ Binary-Search Text Ellipsizing & Extended UTF-8 (`0xC2`):** Replaced linear character stripping in `drawTextEllipsized()` with binary search and added `0xC2` symbol transliteration (`°`, `±`, `²`, `³`, `µ`, `·`, NBSP).
* **Visual Factory Reset Feedback & Restored Build Targets:** Added on-screen progress prompts during the 5-second `USER` button factory reset and restored `companion_radio_usb`, `repeater`, `room_server`, and `kiss_modem` PlatformIO targets.

### 11. Future Optimization Backlog

* **I2S Audio Notification Synthesizer (ES8311 + NS4150B):** Hardware-gated I2S tone generator (`CODEC_EN` IO44 + `PA_CTRL` IO41) for audible message notifications (`UITask::notify()`).
* **Filesystem Migration (`SPIFFS` -> `LittleFS`):** Transition ESP32 storage backend from `SPIFFS` to `LittleFS` for improved flash wear-leveling, faster contact serialization, and power-loss resilience.

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
python -m esptool --port COM15 --baud 921600 --chip esp32s3 write_flash 0x0 release/MeshCore-NM-EPD-420-BW-v1.17.1-mod-rev0.3.bin

# Clean software restart
python -m esptool --port COM15 --before no-reset --after watchdog-reset flash_id
```

Or double-click [`release/flash.bat`](release/flash.bat) on Windows.
