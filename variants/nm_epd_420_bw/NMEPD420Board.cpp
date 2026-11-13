#include "NMEPD420Board.h"

#include <esp_task_wdt.h>
#include <math.h>
#include <target.h>
#ifdef ENABLE_WIFI_INTERFACE
#include <WiFi.h>
#endif
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <driver/usb_serial_jtag.h>
#include <esp_sleep.h>
#include <soc/usb_serial_jtag_struct.h>

#ifndef PIN_SDCARD_CS
#define PIN_SDCARD_CS 7
#endif
#ifndef SX126X_POWER_EN
#define SX126X_POWER_EN 47
#endif
#ifndef BATTERY_PIN
#define BATTERY_PIN 3
#endif
#ifndef PIN_ADC_CTRL
#define PIN_ADC_CTRL 43
#endif
#ifndef ADC_MULTIPLIER
#define ADC_MULTIPLIER 2.0f
#endif

void NMEPD420Board::begin() {
  esp_reset_reason_t reason = esp_reset_reason();
  esp_sleep_wakeup_cause_t wakeup_cause = ESP_SLEEP_WAKEUP_UNDEFINED;
  uint64_t wakeup_source = 0;

  if (reason == ESP_RST_DEEPSLEEP) {
    wakeup_cause = esp_sleep_get_wakeup_cause();
    if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT1) {
      wakeup_source = esp_sleep_get_ext1_wakeup_status();
    }
#ifdef P_LORA_DIO_1
    rtc_gpio_deinit((gpio_num_t)P_LORA_DIO_1);
#endif
#ifdef PIN_BOOT_BTN
    rtc_gpio_deinit((gpio_num_t)PIN_BOOT_BTN);
#endif
  }

#ifdef SX126X_POWER_EN
  // Enable SX1262 rail first (and wait for VDD + 1.8V TCXO stabilization) before raising NSS HIGH
  pinMode(SX126X_POWER_EN, OUTPUT);
  digitalWrite(SX126X_POWER_EN, HIGH);
  gpio_hold_dis((gpio_num_t)SX126X_POWER_EN);
  delay(10);
#endif

  // Snapshot initial USB-Serial/JTAG SOF frame index before USB host activity monitoring begins
  _last_usb_sof_idx = static_cast<uint16_t>(USB_SERIAL_JTAG.fram_num.sof_frame_index);
  _last_usb_sof_ms = 0;
  _usb_sof_seen = false;

#ifdef PIN_BOOT_BTN
  pinMode(PIN_BOOT_BTN, INPUT_PULLUP);
#endif
#ifdef PIN_USER_BTN
  pinMode(PIN_USER_BTN, INPUT_PULLUP);
#endif

  // 1. Establish defined active output drive levels BEFORE releasing pad holds
  // Keeps SPI CS lines pulled HIGH to prevent bus collisions or SPI flash/SD glitching
  pinMode(P_LORA_NSS, OUTPUT);
  digitalWrite(P_LORA_NSS, HIGH);

  pinMode(PIN_SDCARD_CS, OUTPUT);
  digitalWrite(PIN_SDCARD_CS, HIGH);

#ifndef DISPLAY_CLASS
#ifdef PIN_DISPLAY_CS
  pinMode(PIN_DISPLAY_CS, OUTPUT);
  digitalWrite(PIN_DISPLAY_CS, HIGH);
#endif
#ifdef PIN_DISPLAY_RST
  pinMode(PIN_DISPLAY_RST, OUTPUT);
  digitalWrite(PIN_DISPLAY_RST, HIGH);
#endif
#ifdef PIN_DISPLAY_DC
  pinMode(PIN_DISPLAY_DC, OUTPUT);
  digitalWrite(PIN_DISPLAY_DC, LOW);
#endif
#endif

  // Keep audio amplifier disabled (0 current draw), but power codec and sensor rails HIGH to prevent ESD
  // clamping on shared I2C bus (IO39/38)
#ifdef PIN_AMP_ENABLE
  pinMode(PIN_AMP_ENABLE, OUTPUT);
  digitalWrite(PIN_AMP_ENABLE, LOW);
#endif

#ifdef PIN_ES8311_POWER
  pinMode(PIN_ES8311_POWER, OUTPUT);
  digitalWrite(PIN_ES8311_POWER, HIGH);
#endif

#ifdef AHTX0_POWER_PIN
  pinMode(AHTX0_POWER_PIN, OUTPUT);
  digitalWrite(AHTX0_POWER_PIN, HIGH);
#endif

#ifdef PIN_ADC_CTRL
  pinMode(PIN_ADC_CTRL, OUTPUT);
  digitalWrite(PIN_ADC_CTRL, LOW);
#endif

  // 2. Unhold all pads unconditionally across RTC and digital domains on any reset / wakeup
  gpio_deep_sleep_hold_dis();
  rtc_gpio_hold_dis((gpio_num_t)P_LORA_NSS);
  rtc_gpio_hold_dis((gpio_num_t)PIN_SDCARD_CS);
#ifdef AHTX0_POWER_PIN
  gpio_hold_dis((gpio_num_t)AHTX0_POWER_PIN);
#endif
#ifdef PIN_ADC_CTRL
  gpio_hold_dis((gpio_num_t)PIN_ADC_CTRL);
#endif
#ifdef PIN_AMP_ENABLE
  gpio_hold_dis((gpio_num_t)PIN_AMP_ENABLE);
#endif
#ifdef PIN_ES8311_POWER
  gpio_hold_dis((gpio_num_t)PIN_ES8311_POWER);
#endif
#ifdef SX126X_POWER_EN
  gpio_hold_dis((gpio_num_t)SX126X_POWER_EN);
#endif

  // 3. Initialize base ESP32Board now that power rails are stable and pad holds released (prevents I2C ESD clamping)
  ESP32Board::begin();

  if (reason == ESP_RST_DEEPSLEEP) {
    if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT0) {
      startup_reason = BD_STARTUP_NORMAL; // Woken up by BOOT button press
    } else if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT1) {
      if (wakeup_source & (1ULL << P_LORA_DIO_1)) {
        startup_reason = BD_STARTUP_RX_PACKET;
      }
    }
  }

  // Reconfigure task watchdog to 15s to allow blocking E-Paper refreshes (~2.2s) without losing deadlock
  // protection
  esp_task_wdt_config_t twdt_config = {
    .timeout_ms = 15000,
    .idle_core_mask = 0,
    .trigger_panic = true,
  };
  if (esp_task_wdt_reconfigure(&twdt_config) != ESP_OK) {
    esp_task_wdt_init(&twdt_config);
  }

  // Seed cached battery voltage synchronously once at boot so later getBattMilliVolts() calls never stall
  const unsigned long now = millis();
  _cached_batt_mv = readRawBattMilliVolts();
  _last_batt_read_ms = (now == 0) ? 1UL : now;
}

bool NMEPD420Board::setAdcMultiplier(float multiplier) {
  if (isnan(multiplier) || isinf(multiplier) || multiplier < 0.0f || multiplier > 10.0f) {
    return false;
  }
  const float prev = _adc_multiplier;
  const float next = (multiplier == 0.0f) ? ADC_MULTIPLIER : multiplier;
  if (prev > 0.0f && _cached_batt_mv > 0) {
    _cached_batt_mv = static_cast<uint16_t>((static_cast<float>(_cached_batt_mv) * next) / prev);
  }
  _adc_multiplier = next;
  _adc_sample_requested = true;
  return true;
}

void NMEPD420Board::loop() {
  ESP32Board::loop();

  const unsigned long now = millis();
  switch (_adc_state) {
    case BattAdcState::ADC_IDLE:
      if (_adc_sample_requested ||
          (_last_batt_read_ms != 0 && static_cast<unsigned long>(now - _last_batt_read_ms) >= BATT_CACHE_TTL_MS)) {
        _adc_sample_requested = false;
#ifdef PIN_ADC_CTRL
        pinMode(PIN_ADC_CTRL, OUTPUT);
        digitalWrite(PIN_ADC_CTRL, HIGH);
#endif
        _adc_state_ms = now;
        _adc_state = BattAdcState::ADC_SETTLING;
      }
      break;

    case BattAdcState::ADC_SETTLING:
#ifdef PIN_ADC_CTRL
      if (static_cast<unsigned long>(now - _adc_state_ms) < BATT_SETTLE_MS) {
        break;
      }
#endif
      analogReadResolution(12);
      // ESP32-S3 ordering trap: attach pin with a throwaway read before configuring pin attenuation
      (void)analogReadMilliVolts(BATTERY_PIN);
      analogSetPinAttenuation(BATTERY_PIN, ADC_11db);
      _adc_accum_mv = 0;
      _adc_sample_count = 0;
      _adc_sample_us = micros();
      _adc_state = BattAdcState::ADC_SAMPLING;
      break;

    case BattAdcState::ADC_SAMPLING:
      if (_adc_sample_count == 0 || static_cast<unsigned long>(micros() - _adc_sample_us) >= ADC_SAMPLE_GAP_US) {
        _adc_accum_mv += analogReadMilliVolts(BATTERY_PIN);
        _adc_sample_us = micros();
        _adc_sample_count++;
        if (_adc_sample_count >= ADC_NUM_SAMPLES) {
#ifdef PIN_ADC_CTRL
          digitalWrite(PIN_ADC_CTRL, LOW);
#endif
          uint32_t pin_mv = _adc_accum_mv / ADC_NUM_SAMPLES;
          _cached_batt_mv = static_cast<uint16_t>(static_cast<float>(pin_mv) * _adc_multiplier);
          _last_batt_read_ms = (now == 0) ? 1UL : now;
          _adc_state = BattAdcState::ADC_IDLE;
        }
      }
      break;
  }
}

uint16_t NMEPD420Board::sampleBattAdc() {
  analogReadResolution(12);
  // ESP32-S3 ordering trap: attach pin with a throwaway read before configuring pin attenuation
  (void)analogReadMilliVolts(BATTERY_PIN);
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db);

  // Average 8 samples with 150 us gap to filter out RF ripple and power supply noise
  uint32_t sum = 0;
  for (uint8_t i = 0; i < ADC_NUM_SAMPLES; i++) {
    sum += analogReadMilliVolts(BATTERY_PIN);
    delayMicroseconds(ADC_SAMPLE_GAP_US);
  }

  uint32_t pin_mv = sum / ADC_NUM_SAMPLES;
  return static_cast<uint16_t>(static_cast<float>(pin_mv) * _adc_multiplier);
}

uint16_t NMEPD420Board::getBattMilliVolts() {
  const unsigned long now = millis();
  if (_last_batt_read_ms == 0) {
    _cached_batt_mv = readRawBattMilliVolts();
    _last_batt_read_ms = (now == 0) ? 1UL : now;
    return _cached_batt_mv;
  }
  if (static_cast<unsigned long>(now - _last_batt_read_ms) >= BATT_CACHE_TTL_MS) {
    _adc_sample_requested = true;
  }
  return _cached_batt_mv;
}

uint16_t NMEPD420Board::readRawBattMilliVolts() {
  // Abort any in-flight async sample cleanly before taking an explicit synchronous measurement
  _adc_state = BattAdcState::ADC_IDLE;
  _adc_sample_requested = false;
#ifdef PIN_ADC_CTRL
  pinMode(PIN_ADC_CTRL, OUTPUT);
  digitalWrite(PIN_ADC_CTRL, HIGH);
  // Settle divider and decoupling RC filter (20 ms required per hardware validation)
  delay(BATT_SETTLE_MS);
#endif

  uint16_t mv = sampleBattAdc();

#ifdef PIN_ADC_CTRL
  digitalWrite(PIN_ADC_CTRL, LOW);
#endif

  return mv;
}

struct LipoPoint {
  uint16_t mv;
  uint8_t pct;
};

static const LipoPoint LIPO_TABLE[] = { { 4200, 100 }, { 4120, 90 }, { 4030, 80 }, { 3950, 70 },
                                        { 3880, 60 },  { 3820, 50 }, { 3770, 40 }, { 3720, 30 },
                                        { 3650, 20 },  { 3550, 10 }, { 3400, 5 },  { 3300, 0 } };

uint8_t NMEPD420Board::estimateLipoPercentage(uint16_t milliVolts) {
  constexpr size_t NUM_POINTS = sizeof(LIPO_TABLE) / sizeof(LIPO_TABLE[0]);
  if (milliVolts >= LIPO_TABLE[0].mv) return 100;
  if (milliVolts <= LIPO_TABLE[NUM_POINTS - 1].mv) return 0;

  for (size_t i = 0; i < NUM_POINTS - 1; ++i) {
    if (milliVolts >= LIPO_TABLE[i + 1].mv) {
      uint16_t v_high = LIPO_TABLE[i].mv;
      uint16_t v_low = LIPO_TABLE[i + 1].mv;
      uint8_t p_high = LIPO_TABLE[i].pct;
      uint8_t p_low = LIPO_TABLE[i + 1].pct;
      uint32_t num = (uint32_t)(milliVolts - v_low) * (p_high - p_low);
      uint32_t den = (uint32_t)(v_high - v_low);
      uint8_t pct = p_low + static_cast<uint8_t>((num + (den / 2U)) / den);
      return pct > 100 ? 100 : pct;
    }
  }
  return 0;
}

const char *NMEPD420Board::getManufacturerName() const {
  return "RockBase IoT NM-EPD-420-BW";
}

bool NMEPD420Board::isUsbHostActive() {
  const unsigned long now = millis();
  const uint16_t sof_idx = static_cast<uint16_t>(USB_SERIAL_JTAG.fram_num.sof_frame_index);
  // SOF frame index increments every 1ms only when an active USB host is polling;
  // usb_serial_jtag_is_connected() starts true for the first 3 ticks after reset even on battery.
  if (sof_idx != _last_usb_sof_idx || Serial.available() > 0 || (now > 50UL && usb_serial_jtag_is_connected())) {
    _last_usb_sof_idx = sof_idx;
    _last_usb_sof_ms = now;
    _usb_sof_seen = true;
    return true;
  }
  return _usb_sof_seen && ((now - _last_usb_sof_ms) < USB_HOST_HOLD_MS);
}

void NMEPD420Board::powerOff() {
  enterDeepSleep(0);
}

void NMEPD420Board::sleep(uint32_t secs) {
#ifdef DISPLAY_CLASS
  if (display.isBusy()) {
    delay(10);
    return;
  }
#endif
  if (isUsbHostActive()) {
    delay(1);
    return;
  }
  Serial.flush();

  // Prebuilt Arduino qio_opi libs compile CONFIG_PM_POWER_DOWN_CPU_IN_LIGHT_SLEEP=y without
  // CONFIG_PM_SLP_SPIRAM_HALFSLEEP_ENABLED, so esp_sleep_start() unconditionally enables CPU +
  // tagmem retention in Light Sleep (locking up OPI PSRAM cache on wake regardless of ESP_PD_DOMAIN_CPU).
  // Sleep in interruptible WFI slices instead: keeps SX1262 RX active, feeds TWDT, and wakes <=20ms
  // on DIO1 RX, BOOT press, or USB host attach.
  const uint32_t sleep_ms = (secs == 0) ? 30000UL : ((secs > 3600UL) ? 3600000UL : (secs * 1000UL));
  const unsigned long start_ms = millis();
  while (static_cast<uint32_t>(millis() - start_ms) < sleep_ms) {
    if (esp_task_wdt_status(NULL) == ESP_OK) {
      esp_task_wdt_reset();
    }
#ifdef P_LORA_DIO_1
    if (digitalRead(P_LORA_DIO_1) == HIGH) {
      break;
    }
#endif
#ifdef PIN_BOOT_BTN
    if (digitalRead(PIN_BOOT_BTN) == LOW) {
      break;
    }
#endif
#ifdef DISPLAY_CLASS
    if (display.isBusy()) {
      break;
    }
#endif
    if (isUsbHostActive()) {
      break;
    }
    delay(20);
  }
}

void NMEPD420Board::enterDeepSleep(uint32_t secs) {
// 1. Power off board peripherals and sensors to eliminate idle current leakage
#ifdef AHTX0_POWER_PIN
  digitalWrite(AHTX0_POWER_PIN, LOW);
  gpio_hold_en((gpio_num_t)AHTX0_POWER_PIN);
#endif

#ifdef PIN_ADC_CTRL
  digitalWrite(PIN_ADC_CTRL, LOW);
  gpio_hold_en((gpio_num_t)PIN_ADC_CTRL);
#endif

#ifdef PIN_AMP_ENABLE
  digitalWrite(PIN_AMP_ENABLE, LOW);
  gpio_hold_en((gpio_num_t)PIN_AMP_ENABLE);
#endif

#ifdef PIN_ES8311_POWER
  digitalWrite(PIN_ES8311_POWER, LOW);
  gpio_hold_en((gpio_num_t)PIN_ES8311_POWER);
#endif

// 2. Shut down E-Paper display (bistable panel holds image indefinitely with 0 power)
#ifdef DISPLAY_CLASS
  display.turnOff();
#endif

  // 3. Power off LoRa modem, isolate SX1262 signal pins to avoid ESD back-powering when VDD is cut,
  //    keep shared HSPI microSD CS HIGH via RTC hold, then cut SX126X_POWER_EN
  radio_driver.powerOff();
  digitalWrite(PIN_SDCARD_CS, HIGH);
  rtc_gpio_hold_en((gpio_num_t)PIN_SDCARD_CS);

#ifdef SX126X_POWER_EN
  pinMode(P_LORA_SCLK, INPUT);
  pinMode(P_LORA_MOSI, INPUT);
  pinMode(P_LORA_RESET, INPUT);
  digitalWrite(P_LORA_NSS, LOW);
  rtc_gpio_hold_en((gpio_num_t)P_LORA_NSS);
  digitalWrite(SX126X_POWER_EN, LOW);
  gpio_hold_en((gpio_num_t)SX126X_POWER_EN);
#else
  digitalWrite(P_LORA_NSS, HIGH);
  rtc_gpio_hold_en((gpio_num_t)P_LORA_NSS);
#endif

  // Latch digital GPIO pad hold states during deep sleep
  gpio_deep_sleep_hold_en();

  // 4. Power off GPS and WiFi if active
  if (sensors.getLocationProvider() != NULL) {
    sensors.getLocationProvider()->stop();
  }
#ifdef ENABLE_WIFI_INTERFACE
  if (WiFi.getMode() != WIFI_OFF) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }
#endif

  // 5. Flush serial buffers
  Serial.flush();
  delay(50);

  // 6. Clear stale wakeup sources
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

  // 7. Enable RTC peripheral power domain so ext0 / ext1 wakeups trigger
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

// 8. Configure Hardware Button Wakeup on PIN_BOOT_BTN (GPIO 0):
// BOOT button is pulled HIGH externally; pressing pulls it LOW (0).
#ifdef PIN_BOOT_BTN
  rtc_gpio_init((gpio_num_t)PIN_BOOT_BTN);
  rtc_gpio_set_direction((gpio_num_t)PIN_BOOT_BTN, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en((gpio_num_t)PIN_BOOT_BTN);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_BOOT_BTN);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BOOT_BTN, 0); // Active LOW
#endif

  // 9. LoRa Packet Wakeup on DIO1 is unavailable in Deep Sleep when SX126X_POWER_EN cuts SX1262 VCC
  //    (for RX packet wakeup, NMEPD420Board::sleep() uses Light Sleep via ESP32Board::sleep)
#if defined(P_LORA_DIO_1) && !defined(SX126X_POWER_EN)
  rtc_gpio_init((gpio_num_t)P_LORA_DIO_1);
  rtc_gpio_set_direction((gpio_num_t)P_LORA_DIO_1, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pulldown_en((gpio_num_t)P_LORA_DIO_1);
  esp_sleep_enable_ext1_wakeup((1ULL << P_LORA_DIO_1), ESP_EXT1_WAKEUP_ANY_HIGH);
#endif

  // 10. Configure Timer Wakeup if requested
  if (secs > 0) {
    esp_sleep_enable_timer_wakeup((uint64_t)secs * 1000000ULL);
  }

  // 11. Halt CPU until wake/reset
  esp_deep_sleep_start();
}
