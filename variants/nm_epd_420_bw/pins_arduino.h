#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

// Native USB-CDC is used for serial console.
// Note: GPIO 43 is dedicated to ADC_EN (battery divider gate) and GPIO 44 to CODEC_EN (audio power gate).
// TX and RX are not exposed on GPIO 43/44 to prevent accidental hardware pin reconfiguration.

// I²C (AHT20 + ES8311 codec share this bus)
static const uint8_t SDA = 39;
static const uint8_t SCL = 38;

// HSPI bus shared by LoRa modem and µSD card (LoRa is the only consumer MeshCore uses)
static const uint8_t SS = 8;
static const uint8_t MOSI = 10;
static const uint8_t MISO = 11;
static const uint8_t SCK = 9;

// SX126x control lines
// static const uint8_t RST_LoRa = 12;
// static const uint8_t BUSY_LoRa = 13;
// static const uint8_t DIO1 = 14;

#endif
