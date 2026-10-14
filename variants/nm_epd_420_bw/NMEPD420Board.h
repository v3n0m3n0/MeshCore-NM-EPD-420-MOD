#pragma once

#include <Arduino.h>
#include <helpers/ESP32Board.h>

class NMEPD420Board : public ESP32Board {
public:
  void begin();
  uint16_t getBattMilliVolts() override;
  const char* getManufacturerName() const override;
};
