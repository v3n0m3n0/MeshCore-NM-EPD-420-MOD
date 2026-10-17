#include "NMEPD420EnvironmentSensorManager.h"

void NMEPD420EnvironmentSensorManager::powerOnSync() {
  #ifdef AHTX0_POWER_PIN
  digitalWrite(AHTX0_POWER_PIN, HIGH);
  delay(AHTX0_POWER_WARMUP_MS);
  #endif
}

void NMEPD420EnvironmentSensorManager::powerOff() {
  #ifdef AHTX0_POWER_PIN
  digitalWrite(AHTX0_POWER_PIN, LOW);
  #endif
  _power_warming = false;
}

void NMEPD420EnvironmentSensorManager::triggerAsyncRefresh() {
  if (_active_sensor_count <= 0 || _power_warming) return;
  #ifdef AHTX0_POWER_PIN
  digitalWrite(AHTX0_POWER_PIN, HIGH);
  _warmup_start_ms = millis();
  _power_warming = true;
  #else
  readIntoCache();
  #endif
}

void NMEPD420EnvironmentSensorManager::readIntoCache() {
  CayenneLPP tmp(32);
  EnvironmentSensorManager::querySensors(TELEM_PERM_ENVIRONMENT, tmp);
  const uint8_t* b = tmp.getBuffer();
  uint8_t sz = tmp.getSize();
  uint8_t idx = 0;
  while (idx + 2 <= sz) {
    uint8_t type = b[idx + 1];
    if (type == LPP_TEMPERATURE && idx + 4 <= sz) {
      int16_t raw_t = static_cast<int16_t>((static_cast<uint16_t>(b[idx + 2]) << 8) | b[idx + 3]);
      _cached_temp = raw_t / 10.0f;
      _has_cache = true;
      idx += 4;
    } else if (type == LPP_RELATIVE_HUMIDITY && idx + 3 <= sz) {
      _cached_hum = b[idx + 2] / 2.0f;
      _has_cache = true;
      idx += 3;
    } else {
      break;
    }
  }
  _last_read_ms = millis();
}

bool NMEPD420EnvironmentSensorManager::begin() {
  powerOnSync();
  const bool result = EnvironmentSensorManager::begin();
  if (result && _active_sensor_count > 0) {
    readIntoCache();
  }
  powerOff();
  return result;
}

void NMEPD420EnvironmentSensorManager::loop() {
  #if ENV_INCLUDE_GPS || defined(ENV_INCLUDE_BME680_BSEC)
  EnvironmentSensorManager::loop();
  #endif
  if (_power_warming && (millis() - _warmup_start_ms >= AHTX0_POWER_WARMUP_MS)) {
    readIntoCache();
    powerOff();
  }
}

bool NMEPD420EnvironmentSensorManager::querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) {
  if (requester_permissions & TELEM_PERM_LOCATION && gps_active) {
    telemetry.addGPS(TELEM_CHANNEL_SELF, node_lat, node_lon, node_altitude);
  }

  if (requester_permissions & TELEM_PERM_ENVIRONMENT) {
    if (!_has_cache && _active_sensor_count > 0) {
      powerOnSync();
      readIntoCache();
      powerOff();
    }
    if (_has_cache) {
      telemetry.addTemperature(TELEM_CHANNEL_SELF + 1, _cached_temp);
      telemetry.addRelativeHumidity(TELEM_CHANNEL_SELF + 1, _cached_hum);
      if (millis() - _last_read_ms >= ENV_CACHE_TTL_MS) {
        triggerAsyncRefresh();
      }
    }
  }
  return true;
}
