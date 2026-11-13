#pragma once

#include <helpers/sensors/EnvironmentSensorManager.h>

class NMEPD420EnvironmentSensorManager : public EnvironmentSensorManager {
public:
  bool begin() override;
  void loop() override;
  bool querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) override;
  bool sampleSensors();
  void triggerAsyncRefresh();

  bool hasCache() const { return _has_cache; }
  float getCachedTemp() const { return _cached_temp; }
  float getCachedHum() const { return _cached_hum; }
  unsigned long getLastReadMs() const { return _last_read_ms; }

private:
  enum class AsyncState : uint8_t {
    IDLE = 0,
    WAIT_WARMUP,
    WAIT_INIT,
    WAIT_MEASURE,
    POLL_READY
  };

  static constexpr unsigned long AUTO_REFRESH_INTERVAL_MS = 45000UL;
  static constexpr unsigned long INIT_WAIT_MS = 15UL;
  static constexpr unsigned long MEASURE_WAIT_MS = 80UL;
  static constexpr unsigned long POLL_INTERVAL_MS = 10UL;
  static constexpr uint8_t MAX_POLL_RETRIES = 5;

  float _cached_temp = 0.0f;
  float _cached_hum = 0.0f;
  bool _has_cache = false;
  bool _rail_powered = false;
  bool _async_requested = false;
  unsigned long _last_read_ms = 0;
  unsigned long _last_attempt_ms = 0;
  unsigned long _state_ts_ms = 0;
  AsyncState _async_state = AsyncState::IDLE;
  uint8_t _retry_count = 0;

  void ensurePowerRailOn();
  bool startAsyncMeasurement();
  bool sendMeasureTrigger();
  bool tryReadMeasurement(bool log_result);
};
