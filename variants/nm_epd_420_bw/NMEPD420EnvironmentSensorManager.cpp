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
  Wire.setTimeOut(50);

  #ifdef AHTX0_POWER_PIN
  pinMode(AHTX0_POWER_PIN, OUTPUT);
  digitalWrite(AHTX0_POWER_PIN, LOW);
  #endif

  // Initial boot sensor reading to populate cache
  sampleSensors();
  return true;
}

void NMEPD420EnvironmentSensorManager::sampleSensors() {
  #ifdef AHTX0_POWER_PIN
  pinMode(AHTX0_POWER_PIN, OUTPUT);
  // Drive TEMP_CTL (GPIO 40) HIGH to power AHT20
  digitalWrite(AHTX0_POWER_PIN, HIGH);
  // Wait 50 ms for power stabilization (cold power-on state)
  delay(AHTX0_POWER_WARMUP_MS);
  #endif

  // Ensure Wire is active on the correct pins with a fast timeout
  #if defined(PIN_BOARD_SDA) && defined(PIN_BOARD_SCL)
  Wire.begin(PIN_BOARD_SDA, PIN_BOARD_SCL);
  #else
  Wire.begin();
  #endif
  Wire.setClock(100000);
  Wire.setTimeOut(50);

  // Probe I2C bus and initialize AHTX0 with retries
  bool init_ok = false;
  for (int attempt = 0; attempt < 3; attempt++) {
    Wire.beginTransmission(TELEM_AHTX_ADDRESS);
    if (Wire.endTransmission() == 0) {
      if (Wire.requestFrom((uint8_t)TELEM_AHTX_ADDRESS, (uint8_t)1) == 1) {
        uint8_t st = Wire.read();
        if (st != 0xFF) {
          if (AHTX0.begin(&Wire)) {
            init_ok = true;
            break;
          }
        }
      }
    }
    delay(10);
  }

  // If initialized, sample temperature and humidity
  if (init_ok) {
    sensors_event_t hum_event, temp_event;
    if (AHTX0.getEvent(&hum_event, &temp_event)) {
      if (!isnan(temp_event.temperature) && !isnan(hum_event.relative_humidity)) {
        _cached_temp = temp_event.temperature;
        _cached_hum = hum_event.relative_humidity;
        _has_cache = true;
        _active_sensor_count = 1;
        _last_read_ms = millis();
      }
    }
  }

  #ifdef AHTX0_POWER_PIN
  // Immediately power off AHT20 to eliminate quiescent leakage (0 uA)
  digitalWrite(AHTX0_POWER_PIN, LOW);
  #endif
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
