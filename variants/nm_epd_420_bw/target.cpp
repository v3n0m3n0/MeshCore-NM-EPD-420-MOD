#include "target.h"
#include <Arduino.h>

NMEPD420Board board;
static SPIClass spi(HSPI);
RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, spi);
WRAPPER_CLASS radio_driver(radio, board);

ESP32RTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);
NMEPD420EnvironmentSensorManager sensors;

#ifdef DISPLAY_CLASS
DISPLAY_CLASS display;
MomentaryButton boot_btn(PIN_BOOT_BTN, 1000, true);
MomentaryButton user_btn(PIN_USER_BTN, 1000, true);
#endif

bool radio_init() {
  fallback_clock.begin();
#if defined(PIN_BOARD_SDA) && defined(PIN_BOARD_SCL)
  pinMode(PIN_BOARD_SDA, INPUT_PULLUP);
  pinMode(PIN_BOARD_SCL, INPUT_PULLUP);
  Wire.begin(PIN_BOARD_SDA, PIN_BOARD_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(100);
#else
  Wire.begin();
#endif
  rtc_clock.begin(Wire);
  spi.begin(P_LORA_SCLK, P_LORA_MISO, P_LORA_MOSI, P_LORA_NSS);
  return radio.std_init(&spi);
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng);
}
