#include "NMEPD420Board.h"
#include <esp_task_wdt.h>
#include <target.h>
#ifdef ENABLE_WIFI_INTERFACE
#include <WiFi.h>
#endif

#ifndef PIN_SDCARD_CS
#define PIN_SDCARD_CS 7
#endif

void NMEPD420Board::begin() {
  ESP32Board::begin();

  // Reconfigure task watchdog to 15s to allow blocking E-Paper refreshes (~2.2s) without losing deadlock protection
  esp_task_wdt_config_t twdt_config = {
      .timeout_ms = 15000,
      .idle_core_mask = 0,
      .trigger_panic = true,
  };
  if (esp_task_wdt_reconfigure(&twdt_config) != ESP_OK) {
    esp_task_wdt_init(&twdt_config);
  }

  #ifdef SX126X_POWER_EN
  pinMode(SX126X_POWER_EN, OUTPUT);
  digitalWrite(SX126X_POWER_EN, HIGH);
  #endif

  // Crucial: keep audio PA and codec disabled to prevent current draw & brownout resets
  #ifdef PIN_AMP_ENABLE
  pinMode(PIN_AMP_ENABLE, OUTPUT);
  digitalWrite(PIN_AMP_ENABLE, LOW);
  #endif

  #ifdef PIN_ES8311_POWER
  pinMode(PIN_ES8311_POWER, OUTPUT);
  digitalWrite(PIN_ES8311_POWER, LOW);
  #endif

  #ifdef AHTX0_POWER_PIN
  pinMode(AHTX0_POWER_PIN, OUTPUT);
  digitalWrite(AHTX0_POWER_PIN, LOW);
  #endif

  #ifdef PIN_ADC_CTRL
  pinMode(PIN_ADC_CTRL, OUTPUT);
  digitalWrite(PIN_ADC_CTRL, LOW);
  #endif

  esp_reset_reason_t reason = esp_reset_reason();
  if (reason == ESP_RST_DEEPSLEEP) {
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_EXT0) {
      startup_reason = BD_STARTUP_NORMAL; // Woken up by BOOT button press
    } else if (cause == ESP_SLEEP_WAKEUP_EXT1) {
      uint64_t wakeup_source = esp_sleep_get_ext1_wakeup_status();
      if (wakeup_source & (1ULL << P_LORA_DIO_1)) {
        startup_reason = BD_STARTUP_RX_PACKET;
      }
    }
    rtc_gpio_hold_dis((gpio_num_t)P_LORA_NSS);
    rtc_gpio_hold_dis((gpio_num_t)PIN_SDCARD_CS);
    #ifdef AHTX0_POWER_PIN
    rtc_gpio_hold_dis((gpio_num_t)AHTX0_POWER_PIN);
    #endif
    #ifdef PIN_ADC_CTRL
    rtc_gpio_hold_dis((gpio_num_t)PIN_ADC_CTRL);
    #endif
    #ifdef PIN_AMP_ENABLE
    rtc_gpio_hold_dis((gpio_num_t)PIN_AMP_ENABLE);
    #endif
    #ifdef PIN_ES8311_POWER
    rtc_gpio_hold_dis((gpio_num_t)PIN_ES8311_POWER);
    #endif
    #ifdef P_LORA_DIO_1
    rtc_gpio_deinit((gpio_num_t)P_LORA_DIO_1);
    #endif
    #ifdef PIN_BOOT_BTN
    rtc_gpio_deinit((gpio_num_t)PIN_BOOT_BTN);
    #endif
  }

  // Keep shared HSPI MicroSD CS (GPIO 7) de-asserted HIGH so an inserted SD card never collides with SX1262 LoRa
  pinMode(PIN_SDCARD_CS, OUTPUT);
  digitalWrite(PIN_SDCARD_CS, HIGH);
}

uint16_t NMEPD420Board::sampleBattAdc() {
  analogReadResolution(12);
  // ESP32-S3 ordering trap: attach pin with a throwaway read before configuring pin attenuation
  (void)analogReadMilliVolts(BATTERY_PIN);
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db);

  // Average 8 samples with 150 us gap to filter out RF ripple and power supply noise without blocking FreeRTOS
  uint32_t sum = 0;
  for (uint8_t i = 0; i < 8; i++) {
    sum += analogReadMilliVolts(BATTERY_PIN);
    delayMicroseconds(150);
  }

  #ifdef PIN_ADC_CTRL
  digitalWrite(PIN_ADC_CTRL, LOW);
  #endif

  uint32_t pin_mv = sum / 8U;
  return static_cast<uint16_t>(pin_mv * ADC_MULTIPLIER);
}

uint16_t NMEPD420Board::getBattMilliVolts() {
  // Cold start: perform one synchronous read so initial boot UI has valid battery telemetry immediately
  if (_cached_batt_mv == 0) {
    _cached_batt_mv = readRawBattMilliVolts();
    _last_batt_read_ms = millis();
    _adc_settling = false;
    return _cached_batt_mv;
  }

  unsigned long now = millis();
  if (_adc_settling) {
    if (now - _adc_settle_start_ms >= BATT_SETTLE_MS) {
      _cached_batt_mv = sampleBattAdc();
      _last_batt_read_ms = now;
      _adc_settling = false;
    }
    return _cached_batt_mv;
  }

  if (now - _last_batt_read_ms < BATT_CACHE_TTL_MS) {
    return _cached_batt_mv;
  }

  // TTL expired: start non-blocking 20ms RC divider settle phase and return cached value immediately
  #ifdef PIN_ADC_CTRL
  pinMode(PIN_ADC_CTRL, OUTPUT);
  digitalWrite(PIN_ADC_CTRL, HIGH);
  _adc_settle_start_ms = now;
  _adc_settling = true;
  return _cached_batt_mv;
  #else
  _cached_batt_mv = sampleBattAdc();
  _last_batt_read_ms = now;
  return _cached_batt_mv;
  #endif
}

uint16_t NMEPD420Board::readRawBattMilliVolts() {
  #ifdef PIN_ADC_CTRL
  pinMode(PIN_ADC_CTRL, OUTPUT);
  digitalWrite(PIN_ADC_CTRL, HIGH);
  // Settle divider and decoupling RC filter (20 ms required per hardware validation)
  delay(BATT_SETTLE_MS);
  #endif

  return sampleBattAdc();
}

struct LipoPoint {
  uint16_t mv;
  uint8_t pct;
};

static const LipoPoint LIPO_TABLE[] = {
  { 4200, 100 },
  { 4120,  90 },
  { 4030,  80 },
  { 3950,  70 },
  { 3880,  60 },
  { 3820,  50 },
  { 3770,  40 },
  { 3720,  30 },
  { 3650,  20 },
  { 3550,  10 },
  { 3400,   5 },
  { 3300,   0 }
};

uint8_t NMEPD420Board::estimateLipoPercentage(uint16_t milliVolts) {
  constexpr size_t NUM_POINTS = sizeof(LIPO_TABLE) / sizeof(LIPO_TABLE[0]);
  if (milliVolts >= LIPO_TABLE[0].mv) return 100;
  if (milliVolts <= LIPO_TABLE[NUM_POINTS - 1].mv) return 0;

  for (size_t i = 0; i < NUM_POINTS - 1; ++i) {
    if (milliVolts >= LIPO_TABLE[i + 1].mv) {
      uint16_t v_high = LIPO_TABLE[i].mv;
      uint16_t v_low  = LIPO_TABLE[i + 1].mv;
      uint8_t  p_high = LIPO_TABLE[i].pct;
      uint8_t  p_low  = LIPO_TABLE[i + 1].pct;
      uint32_t num = (uint32_t)(milliVolts - v_low) * (p_high - p_low);
      uint32_t den = (uint32_t)(v_high - v_low);
      uint8_t pct = p_low + static_cast<uint8_t>((num + (den / 2U)) / den);
      return pct > 100 ? 100 : pct;
    }
  }
  return 0;
}

const char* NMEPD420Board::getManufacturerName() const {
  return "RockBase IoT NM-EPD-420-BW";
}

void NMEPD420Board::powerOff() {
  enterDeepSleep(0);
}

void NMEPD420Board::sleep(uint32_t secs) {
  enterDeepSleep(secs);
}

void NMEPD420Board::enterDeepSleep(uint32_t secs) {
  // 1. Power off board peripherals and sensors to eliminate idle current leakage
  #ifdef AHTX0_POWER_PIN
  digitalWrite(AHTX0_POWER_PIN, LOW);
  rtc_gpio_hold_en((gpio_num_t)AHTX0_POWER_PIN);
  #endif

  #ifdef PIN_ADC_CTRL
  digitalWrite(PIN_ADC_CTRL, LOW);
  rtc_gpio_hold_en((gpio_num_t)PIN_ADC_CTRL);
  #endif

  #ifdef PIN_AMP_ENABLE
  digitalWrite(PIN_AMP_ENABLE, LOW);
  rtc_gpio_hold_en((gpio_num_t)PIN_AMP_ENABLE);
  #endif

  #ifdef PIN_ES8311_POWER
  digitalWrite(PIN_ES8311_POWER, LOW);
  rtc_gpio_hold_en((gpio_num_t)PIN_ES8311_POWER);
  #endif

  // 2. Shut down E-Paper display (bistable panel holds image indefinitely with 0 power)
  #ifdef DISPLAY_CLASS
  display.turnOff();
  #endif

  // 3. Power off LoRa modem, latch NSS and shared HSPI SD_CS HIGH via RTC hold
  radio_driver.powerOff();
  digitalWrite(P_LORA_NSS, HIGH);
  rtc_gpio_hold_en((gpio_num_t)P_LORA_NSS);
  digitalWrite(PIN_SDCARD_CS, HIGH);
  rtc_gpio_hold_en((gpio_num_t)PIN_SDCARD_CS);

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

  // 9. Configure LoRa Packet Wakeup on DIO1 (GPIO 14):
  #ifdef P_LORA_DIO_1
  rtc_gpio_init((gpio_num_t)P_LORA_DIO_1);
  rtc_gpio_set_direction((gpio_num_t)P_LORA_DIO_1, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pulldown_en((gpio_num_t)P_LORA_DIO_1);
  esp_sleep_enable_ext1_wakeup((1ULL << P_LORA_DIO_1), ESP_EXT1_WAKEUP_ANY_HIGH);
  #endif

  // 10. Configure Timer Wakeup if requested
  if (secs > 0) {
    esp_sleep_enable_timer_wakeup((uint64_t)secs * 1000000ULL);
  }

  // 11. Enter deep sleep (CPU halts here and never returns)
  esp_deep_sleep_start();
}
