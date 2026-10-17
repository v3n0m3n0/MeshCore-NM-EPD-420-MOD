#pragma once

#include <Arduino.h>
#include <helpers/ESP32Board.h>

class NMEPD420Board : public ESP32Board {
private:
  uint16_t _cached_batt_mv = 0;
  unsigned long _last_batt_read_ms = 0;
  unsigned long _adc_settle_start_ms = 0;
  bool _adc_settling = false;
  static constexpr unsigned long BATT_CACHE_TTL_MS = 20000; // 20s TTL cache
  static constexpr unsigned long BATT_SETTLE_MS = 20;       // 20ms RC divider stabilization

  uint16_t sampleBattAdc();

public:
  void begin();
  uint16_t getBattMilliVolts() override;
  uint16_t readRawBattMilliVolts();
  static uint8_t estimateLipoPercentage(uint16_t milliVolts);
  const char* getManufacturerName() const override;

  void powerOff() override;
  void sleep(uint32_t secs) override;
  void enterDeepSleep(uint32_t secs);
};
