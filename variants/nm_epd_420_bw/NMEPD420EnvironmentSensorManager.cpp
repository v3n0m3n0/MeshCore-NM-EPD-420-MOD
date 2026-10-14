#include "NMEPD420EnvironmentSensorManager.h"

void NMEPD420EnvironmentSensorManager::powerOn() {
  #ifdef AHTX0_POWER_PIN
  digitalWrite(AHTX0_POWER_PIN, HIGH);
  delay(AHTX0_POWER_WARMUP_MS);
  #endif
}

void NMEPD420EnvironmentSensorManager::powerOff() {
  #ifdef AHTX0_POWER_PIN
  digitalWrite(AHTX0_POWER_PIN, LOW);
  #endif
}

bool NMEPD420EnvironmentSensorManager::begin() {
  powerOn();
  const bool result = EnvironmentSensorManager::begin();
  powerOff();
  return result;
}

bool NMEPD420EnvironmentSensorManager::querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) {
  powerOn();
  const bool result = EnvironmentSensorManager::querySensors(requester_permissions, telemetry);
  powerOff();
  return result;
}
