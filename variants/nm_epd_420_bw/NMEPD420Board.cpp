#include "NMEPD420Board.h"
#include <esp_task_wdt.h>

void NMEPD420Board::begin() {
  ESP32Board::begin();

  // Deinit task watchdog to avoid triggers during long blocking E-Paper refreshes (~2.2s)
  esp_task_wdt_deinit();

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
    long wakeup_source = esp_sleep_get_ext1_wakeup_status();
    if (wakeup_source & (1L << P_LORA_DIO_1)) {
      startup_reason = BD_STARTUP_RX_PACKET;
    }
    rtc_gpio_hold_dis((gpio_num_t)P_LORA_NSS);
    rtc_gpio_deinit((gpio_num_t)P_LORA_DIO_1);
  }
}

uint16_t NMEPD420Board::getBattMilliVolts() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  #ifdef PIN_ADC_CTRL
  digitalWrite(PIN_ADC_CTRL, HIGH);
  delay(10);
  #endif

  uint32_t raw = 0;
  for (uint8_t i = 0; i < 4; i++) {
    raw += analogReadMilliVolts(BATTERY_PIN);
  }
  #ifdef PIN_ADC_CTRL
  digitalWrite(PIN_ADC_CTRL, LOW);
  #endif

  return static_cast<uint16_t>((raw / 4U) * ADC_MULTIPLIER);
}

const char* NMEPD420Board::getManufacturerName() const {
  return "RockBase IoT NM-EPD-420-BW";
}
