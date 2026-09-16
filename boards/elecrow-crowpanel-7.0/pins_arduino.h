#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include "soc/soc_caps.h"
#include <stdint.h>

#ifndef DEVICE_NAME
#define DEVICE_NAME "Elecrow CrowPanel 7.0 HMI"
#endif

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 19;
static const uint8_t SCL = 20;

// Placeholder values since there is no SPI setup
static const uint8_t SS = -1;
static const uint8_t MOSI = -1;
static const uint8_t MISO = -1;
static const uint8_t SCK = -1;

#endif /* Pins_Arduino_h */
