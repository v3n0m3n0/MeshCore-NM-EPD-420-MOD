#pragma once

#include <Arduino.h>
#include <helpers/ESP32Board.h>

class NMEPD420Board : public ESP32Board {
public:
  void begin();
  uint16_t getBattMilliVolts() override;
  static uint8_t estimateLipoPercentage(uint16_t milliVolts);
  const char* getManufacturerName() const override;

  void powerOff() override;
  void sleep(uint32_t secs) override;
  void enterDeepSleep(uint32_t secs);
};
