#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include "soc/usb_serial_jtag_reg.h"

class LGFX_CrowPanel70 : public lgfx::LGFX_Device {
  lgfx::Bus_RGB     _bus_instance;
  lgfx::Panel_RGB   _panel_instance;
  lgfx::Light_PWM   _light_instance;
  lgfx::Touch_GT911 _touch_instance;

  uint32_t _cur_textcolor = 0xFFFF;
  uint32_t _cur_textbgcolor = 0x0000;

public:
  LGFX_CrowPanel70(void) {
    // USB-JTAG-Pads freigeben, damit GPIO 19 & 20 als I2C laufen
    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);

    {
      auto cfg = _bus_instance.config();
      cfg.panel = &_panel_instance;

      // 16-Bit Parallelbus (B0..B4, G0..G5, R0..R4)
      cfg.pin_d0  = 15; cfg.pin_d1  = 7;  cfg.pin_d2  = 6;  cfg.pin_d3  = 5;  cfg.pin_d4  = 4;
      cfg.pin_d5  = 9;  cfg.pin_d6  = 46; cfg.pin_d7  = 3;  cfg.pin_d8  = 8;  cfg.pin_d9  = 16; cfg.pin_d10 = 1;
      cfg.pin_d11 = 14; cfg.pin_d12 = 21; cfg.pin_d13 = 47; cfg.pin_d14 = 48; cfg.pin_d15 = 45;

      cfg.pin_henable = 41; // DE
      cfg.pin_vsync   = 40; // VSYNC
      cfg.pin_hsync   = 39; // HSYNC
      cfg.pin_pclk    = 0;  // PCLK

      cfg.freq_write = 12000000; // 12 MHz PCLK
      cfg.hsync_polarity = 0;
      cfg.vsync_polarity = 0;
      cfg.pclk_idle_high = 1;

      _bus_instance.config(cfg);
    }

    {
      auto cfg = _panel_instance.config();
      cfg.memory_width  = 800;
      cfg.memory_height = 480;
      cfg.panel_width   = 800;
      cfg.panel_height  = 480;

      cfg.hsync_pulse_width = 48;
      cfg.hsync_back_porch  = 40;
      cfg.hsync_front_porch = 40;
      cfg.vsync_pulse_width = 31;
      cfg.vsync_back_porch  = 13;
      cfg.vsync_front_porch = 1;

      _panel_instance.config(cfg);
    }

    {
      // Beleuchtung per PWM an GPIO 2
      auto cfg = _light_instance.config();
      cfg.pin_bl      = 2;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 7;
      _light_instance.config(cfg);
      _panel_instance.light(&_light_instance);
    }

    {
      // GT911 Touch
      auto cfg = _touch_instance.config();
      cfg.x_min      = 0;
      cfg.x_max      = 799;
      cfg.y_min      = 0;
      cfg.y_max      = 479;
      cfg.pin_sda    = 19;
      cfg.pin_scl    = 20;
      cfg.pin_int    = -1;
      cfg.pin_rst    = -1;
      cfg.i2c_port   = 0;
      cfg.i2c_addr   = 0x5D;
      cfg.freq       = 400000;
      cfg.bus_shared = false;
      _touch_instance.config(cfg);
      _panel_instance.touch(&_touch_instance);
    }

    setPanel(&_panel_instance);
  }

  // Kompatibilitätsmethoden für Bruce
  void setTextColor(uint32_t c) {
    _cur_textcolor = c;
    lgfx::LGFX_Device::setTextColor(c);
  }
  void setTextColor(uint32_t c, uint32_t b) {
    _cur_textcolor = c;
    _cur_textbgcolor = b;
    lgfx::LGFX_Device::setTextColor(c, b);
  }
  uint32_t getTextColor(void) const { return _cur_textcolor; }
  uint32_t getTextBgColor(void) const { return _cur_textbgcolor; }
  void writecommand(uint8_t) {}
  void setSleepMode(bool) {}
  void imageToBin(uint8_t, const String&, int, int, bool, int) {}
};
