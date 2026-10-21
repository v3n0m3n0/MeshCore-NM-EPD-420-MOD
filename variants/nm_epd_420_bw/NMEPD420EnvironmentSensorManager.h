#pragma once

#include <helpers/sensors/EnvironmentSensorManager.h>

class NMEPD420EnvironmentSensorManager : public EnvironmentSensorManager {
public:
  bool begin() override;
  void loop() override;
  bool querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) override;
  bool sampleSensors();
  void triggerAsyncRefresh() { sampleSensors(); }

  bool hasCache() const { return _has_cache; }
  float getCachedTemp() const { return _cached_temp; }
  float getCachedHum() const { return _cached_hum; }
  unsigned long getLastReadMs() const { return _last_read_ms; }

private:
  float _cached_temp = 0.0f;
  float _cached_hum = 0.0f;
  bool _has_cache = false;
  unsigned long _last_read_ms = 0;
};
