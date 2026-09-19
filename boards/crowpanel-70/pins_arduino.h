#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303a
#define USB_PID 0x1001

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 19;
static const uint8_t SCL = 20;

#define GROVE_SDA 19
#define GROVE_SCL 20

#define BAD_TX 43
#define BAD_RX 44

#define SERIAL_TX 43
#define SERIAL_RX 44

static const uint8_t SS   = 10;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 13;
static const uint8_t SCK  = 12;

#endif /* Pins_Arduino_h */
