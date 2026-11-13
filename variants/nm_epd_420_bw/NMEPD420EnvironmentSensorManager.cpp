#include "NMEPD420EnvironmentSensorManager.h"
#include <Wire.h>

#ifndef AHTX0_POWER_PIN
#define AHTX0_POWER_PIN 40
#endif

#ifndef AHTX0_POWER_WARMUP_MS
#define AHTX0_POWER_WARMUP_MS 50
#endif

#ifndef TELEM_AHTX_ADDRESS
#define TELEM_AHTX_ADDRESS 0x38
#endif

void NMEPD420EnvironmentSensorManager::ensurePowerRailOn() {
#ifdef AHTX0_POWER_PIN
  if (!_rail_powered) {
    pinMode(AHTX0_POWER_PIN, OUTPUT);
    // Drive TEMP_CTL (GPIO 40) HIGH once and keep enabled (Trap #14: shared I2C ESD clamping)
    digitalWrite(AHTX0_POWER_PIN, HIGH);
    delay(AHTX0_POWER_WARMUP_MS);
    _rail_powered = true;
  }
#else
  _rail_powered = true;
#endif
}

bool NMEPD420EnvironmentSensorManager::begin() {
  // Wire.begin() is already called in ESP32Board::begin(); only apply bus speed/timeout once here
  Wire.setClock(100000);
  Wire.setTimeOut(100);

  ensurePowerRailOn();

  // Initial boot sensor reading to populate cache before main loop starts
  return sampleSensors();
}

bool NMEPD420EnvironmentSensorManager::sendMeasureTrigger() {
  Wire.beginTransmission(TELEM_AHTX_ADDRESS);
  Wire.write(0xAC);
  Wire.write(0x33);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) {
    _async_state = AsyncState::IDLE;
    _async_requested = false;
    return false;
  }
  _async_state = AsyncState::WAIT_MEASURE;
  _state_ts_ms = millis();
  _retry_count = 0;
  return true;
}

bool NMEPD420EnvironmentSensorManager::startAsyncMeasurement() {
  const unsigned long now = millis();
  _last_attempt_ms = now;

#ifdef AHTX0_POWER_PIN
  if (!_rail_powered) {
    pinMode(AHTX0_POWER_PIN, OUTPUT);
    digitalWrite(AHTX0_POWER_PIN, HIGH);
    _async_state = AsyncState::WAIT_WARMUP;
    _state_ts_ms = now;
    return true;
  }
#endif

  // 1. Probe address 0x38
  Wire.beginTransmission(TELEM_AHTX_ADDRESS);
  if (Wire.endTransmission() != 0) {
    _async_state = AsyncState::IDLE;
    _async_requested = false;
    return false;
  }

  // 2. Query status (command 0x71)
  uint8_t status = 0;
  Wire.beginTransmission(TELEM_AHTX_ADDRESS);
  Wire.write(0x71);
  if (Wire.endTransmission() == 0 && Wire.requestFrom((uint8_t)TELEM_AHTX_ADDRESS, (uint8_t)1) == 1) {
    status = Wire.read();
  }

  // 3. If uncalibrated (Bit 3 == 0), send AHT20 init command [0xBE, 0x08, 0x00] and wait 15ms async
  if ((status & 0x08) == 0) {
    Wire.beginTransmission(TELEM_AHTX_ADDRESS);
    Wire.write(0xBE);
    Wire.write(0x08);
    Wire.write(0x00);
    Wire.endTransmission();
    _async_state = AsyncState::WAIT_INIT;
    _state_ts_ms = now;
    return true;
  }

  return sendMeasureTrigger();
}

bool NMEPD420EnvironmentSensorManager::tryReadMeasurement(bool log_result) {
  uint8_t data[6] = {0};
  if (Wire.requestFrom((uint8_t)TELEM_AHTX_ADDRESS, (uint8_t)6) == 6) {
    for (int i = 0; i < 6; i++) {
      data[i] = Wire.read();
    }
    if ((data[0] & 0x80) == 0) {
      uint32_t hum_raw = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
      uint32_t temp_raw = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];

      float hum = (hum_raw * 100.0f) / 1048576.0f;
      float temp = ((temp_raw * 200.0f) / 1048576.0f) - 50.0f;

      if (temp >= -40.0f && temp <= 85.0f && hum >= 0.0f && hum <= 100.0f) {
        _cached_temp = temp;
        _cached_hum = hum;
        _has_cache = true;
        _active_sensor_count = 1;
        _last_read_ms = millis();
        if (log_result) {
          Serial.printf("[AHT20] Read OK: T=%.1f C, H=%.1f %%\n", temp, hum);
        }
        return true;
      }
      if (log_result) {
        Serial.printf("[AHT20] Invalid data range: T=%.1f C, H=%.1f %%\n", temp, hum);
      }
      return true; // measurement completed even though out-of-range
    }
  }
  return false;
}

void NMEPD420EnvironmentSensorManager::triggerAsyncRefresh() {
  _async_requested = true;
  if (_async_state == AsyncState::IDLE) {
    startAsyncMeasurement();
  }
}

bool NMEPD420EnvironmentSensorManager::sampleSensors() {
  // Abort any in-flight async sequence cleanly before explicit synchronous read
  _async_state = AsyncState::IDLE;
  _async_requested = false;

  // Trap #14: rail remains HIGH across runtime, avoiding redundant 50ms warmup on repeat reads
  ensurePowerRailOn();

  // 1. Probe address 0x38
  Wire.beginTransmission(TELEM_AHTX_ADDRESS);
  int probe_err = Wire.endTransmission();
  if (probe_err != 0) {
    Serial.printf("[AHT20] Probe failed at 0x%02X, err=%d\n", TELEM_AHTX_ADDRESS, probe_err);
    return false;
  }

  // 2. Query status (command 0x71)
  uint8_t status = 0;
  Wire.beginTransmission(TELEM_AHTX_ADDRESS);
  Wire.write(0x71);
  if (Wire.endTransmission() == 0) {
    if (Wire.requestFrom((uint8_t)TELEM_AHTX_ADDRESS, (uint8_t)1) == 1) {
      status = Wire.read();
    }
  }

  // 3. If uncalibrated (Bit 3 == 0) or uninitialized, send AHT20 init command [0xBE, 0x08, 0x00]
  if ((status & 0x08) == 0) {
    Wire.beginTransmission(TELEM_AHTX_ADDRESS);
    Wire.write(0xBE);
    Wire.write(0x08);
    Wire.write(0x00);
    Wire.endTransmission();
    delay(INIT_WAIT_MS);
  }

  // 4. Trigger measurement [0xAC, 0x33, 0x00]
  Wire.beginTransmission(TELEM_AHTX_ADDRESS);
  Wire.write(0xAC);
  Wire.write(0x33);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) {
    Serial.println("[AHT20] Trigger measurement command failed");
    return false;
  }

  // AHT20 conversion takes ~80ms
  delay(MEASURE_WAIT_MS);

  // 5. Read 6 data bytes, retrying if busy bit (Bit 7) is set
  for (uint8_t retry = 0; retry < MAX_POLL_RETRIES; retry++) {
    if (tryReadMeasurement(true)) {
      return _has_cache;
    }
    delay(POLL_INTERVAL_MS);
  }

  Serial.println("[AHT20] Measurement timeout / busy bit set");
  return false;
}

void NMEPD420EnvironmentSensorManager::loop() {
  #if ENV_INCLUDE_GPS || defined(ENV_INCLUDE_BME680_BSEC)
  EnvironmentSensorManager::loop();
  #endif

  const unsigned long now = millis();
  switch (_async_state) {
    case AsyncState::IDLE:
      if (_async_requested || (static_cast<unsigned long>(now - _last_attempt_ms) >= AUTO_REFRESH_INTERVAL_MS)) {
        startAsyncMeasurement();
      }
      break;

    case AsyncState::WAIT_WARMUP:
      if (static_cast<unsigned long>(now - _state_ts_ms) >= AHTX0_POWER_WARMUP_MS) {
        _rail_powered = true;
        startAsyncMeasurement();
      }
      break;

    case AsyncState::WAIT_INIT:
      if (static_cast<unsigned long>(now - _state_ts_ms) >= INIT_WAIT_MS) {
        sendMeasureTrigger();
      }
      break;

    case AsyncState::WAIT_MEASURE:
      if (static_cast<unsigned long>(now - _state_ts_ms) >= MEASURE_WAIT_MS) {
        _async_state = AsyncState::POLL_READY;
        _state_ts_ms = now;
        _retry_count = 0;
        if (tryReadMeasurement(false)) {
          _async_state = AsyncState::IDLE;
          _async_requested = false;
        } else {
          _retry_count = 1;
        }
      }
      break;

    case AsyncState::POLL_READY:
      if (static_cast<unsigned long>(now - _state_ts_ms) >= POLL_INTERVAL_MS) {
        _state_ts_ms = now;
        if (tryReadMeasurement(false) || _retry_count >= MAX_POLL_RETRIES) {
          _async_state = AsyncState::IDLE;
          _async_requested = false;
        } else {
          _retry_count++;
        }
      }
      break;
  }
}

bool NMEPD420EnvironmentSensorManager::querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) {
  if (requester_permissions & TELEM_PERM_LOCATION && gps_active) {
    telemetry.addGPS(TELEM_CHANNEL_SELF, node_lat, node_lon, node_altitude);
  }

  if (requester_permissions & TELEM_PERM_ENVIRONMENT) {
    if (_has_cache) {
      telemetry.addTemperature(TELEM_CHANNEL_SELF + 1, _cached_temp);
      telemetry.addRelativeHumidity(TELEM_CHANNEL_SELF + 1, _cached_hum);
    }
  }
  return true;
}
