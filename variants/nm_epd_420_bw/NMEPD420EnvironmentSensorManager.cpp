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

bool NMEPD420EnvironmentSensorManager::begin() {
  #if defined(PIN_BOARD_SDA) && defined(PIN_BOARD_SCL)
  Wire.begin(PIN_BOARD_SDA, PIN_BOARD_SCL);
  #else
  Wire.begin();
  #endif
  Wire.setClock(100000);
  Wire.setTimeOut(100);

  #ifdef AHTX0_POWER_PIN
  pinMode(AHTX0_POWER_PIN, OUTPUT);
  digitalWrite(AHTX0_POWER_PIN, HIGH);
  delay(AHTX0_POWER_WARMUP_MS);
  #endif

  // Initial boot sensor reading to populate cache
  return sampleSensors();
}

bool NMEPD420EnvironmentSensorManager::sampleSensors() {
  #ifdef AHTX0_POWER_PIN
  pinMode(AHTX0_POWER_PIN, OUTPUT);
  // Drive TEMP_CTL (GPIO 40) HIGH to power AHT20
  digitalWrite(AHTX0_POWER_PIN, HIGH);
  delay(AHTX0_POWER_WARMUP_MS);
  #endif

  #if defined(PIN_BOARD_SDA) && defined(PIN_BOARD_SCL)
  Wire.begin(PIN_BOARD_SDA, PIN_BOARD_SCL);
  #else
  Wire.begin();
  #endif
  Wire.setClock(100000);
  Wire.setTimeOut(100);

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
    delay(15);
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
  delay(80);

  // 5. Read 6 data bytes, retrying if busy bit (Bit 7) is set
  uint8_t data[6] = {0};
  bool measurement_ready = false;
  for (int retry = 0; retry < 5; retry++) {
    if (Wire.requestFrom((uint8_t)TELEM_AHTX_ADDRESS, (uint8_t)6) == 6) {
      for (int i = 0; i < 6; i++) {
        data[i] = Wire.read();
      }
      if ((data[0] & 0x80) == 0) {
        measurement_ready = true;
        break;
      }
    }
    delay(10);
  }


  if (!measurement_ready) {
    Serial.println("[AHT20] Measurement timeout / busy bit set");
    return false;
  }

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
    Serial.printf("[AHT20] Read OK: T=%.1f C, H=%.1f %%\n", temp, hum);
    return true;
  }

  Serial.printf("[AHT20] Invalid data range: T=%.1f C, H=%.1f %%\n", temp, hum);
  return false;
}

void NMEPD420EnvironmentSensorManager::loop() {
  #if ENV_INCLUDE_GPS || defined(ENV_INCLUDE_BME680_BSEC)
  EnvironmentSensorManager::loop();
  #endif
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
