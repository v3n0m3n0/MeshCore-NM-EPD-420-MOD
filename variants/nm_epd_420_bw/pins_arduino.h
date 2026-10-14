#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

static const uint8_t TX = 43;
static const uint8_t RX = 44;

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
