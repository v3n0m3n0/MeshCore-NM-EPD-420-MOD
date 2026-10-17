#pragma once

#include <helpers/sensors/EnvironmentSensorManager.h>

class NMEPD420EnvironmentSensorManager : public EnvironmentSensorManager {
public:
  bool begin() override;
  void loop() override;
  bool querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) override;
  void triggerAsyncRefresh();

private:
  void powerOnSync();
  void powerOff();
  void readIntoCache();

  float _cached_temp = 0.0f;
  float _cached_hum = 0.0f;
  bool _has_cache = false;
  bool _power_warming = false;
  unsigned long _warmup_start_ms = 0;
  unsigned long _last_read_ms = 0;
  static constexpr unsigned long ENV_CACHE_TTL_MS = 15000;
};
