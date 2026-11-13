#pragma once

#include <Arduino.h>
#include <helpers/ESP32Board.h>

class NMEPD420Board : public ESP32Board {
private:
  enum class BattAdcState : uint8_t {
    ADC_IDLE = 0,
    ADC_SETTLING,
    ADC_SAMPLING
  };

  uint16_t _cached_batt_mv = 0;
  unsigned long _last_batt_read_ms = 0;
  float _adc_multiplier = ADC_MULTIPLIER;
  BattAdcState _adc_state = BattAdcState::ADC_IDLE;
  bool _adc_sample_requested = false;
  unsigned long _adc_state_ms = 0;
  unsigned long _adc_sample_us = 0;
  uint32_t _adc_accum_mv = 0;
  uint8_t _adc_sample_count = 0;

  uint16_t _last_usb_sof_idx = 0;
  unsigned long _last_usb_sof_ms = 0;
  bool _usb_sof_seen = false;
  static constexpr unsigned long BATT_CACHE_TTL_MS = 20000; // 20s TTL cache
  static constexpr unsigned long BATT_SETTLE_MS = 20;       // 20ms RC divider stabilization
  static constexpr unsigned long ADC_SAMPLE_GAP_US = 150;   // 150us gap between 8 ADC samples
  static constexpr uint8_t ADC_NUM_SAMPLES = 8;
  static constexpr unsigned long USB_HOST_HOLD_MS = 5000;   // 5s grace window against transient SOF stalls

  uint16_t sampleBattAdc();

public:
  void begin();
  void loop() override;
  bool isUsbHostActive();
  bool hasUsbPowerDetect() const override { return true; }
  bool isUsbPowered() override { return isUsbHostActive(); }
  bool setAdcMultiplier(float multiplier) override;
  float getAdcMultiplier() const override { return _adc_multiplier; }
  uint16_t getBattMilliVolts() override;
  uint16_t readRawBattMilliVolts();
  static uint8_t estimateLipoPercentage(uint16_t milliVolts);
  const char *getManufacturerName() const override;

  void powerOff() override;
  void sleep(uint32_t secs) override;
  void enterDeepSleep(uint32_t secs);
};
