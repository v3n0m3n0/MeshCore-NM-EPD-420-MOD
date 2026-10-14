#pragma once

#include <helpers/sensors/EnvironmentSensorManager.h>

class NMEPD420EnvironmentSensorManager : public EnvironmentSensorManager {
public:
  bool begin() override;
  bool querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) override;

private:
  void powerOn();
  void powerOff();
};
