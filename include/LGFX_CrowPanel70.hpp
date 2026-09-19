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

  uint32_t _cur_textcolor   = 0xFFFF;
  uint32_t _cur_textbgcolor = 0x0000;
  bool     _swapBytes       = false;
  uint8_t  _cur_textsize    = 1;
  uint8_t  _cur_textdatum   = 0;
  uint8_t  _cur_rotation    = 0;

public:
  LGFX_CrowPanel70(void) {
    // USB-JTAG-Pads freigeben, damit GPIO 19 & 20 als I2C (GT911) antworten
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
      cfg.hsync_polarity    = 0;
      cfg.hsync_front_porch = 40;
      cfg.hsync_pulse_width = 48;
      cfg.hsync_back_porch  = 40;
      cfg.vsync_polarity    = 0;
      cfg.vsync_front_porch = 1;
      cfg.vsync_pulse_width = 31;
      cfg.vsync_back_porch  = 13;
      cfg.pclk_idle_high    = 1;

      _bus_instance.config(cfg);
    }

    {
      auto cfg = _panel_instance.config();
      cfg.memory_width  = 800;
      cfg.memory_height = 480;
      cfg.panel_width   = 800;
      cfg.panel_height  = 480;
      cfg.offset_x      = 0;
      cfg.offset_y      = 0;
      _panel_instance.config(cfg);
    }
    {
      auto cfg = _panel_instance.config_detail();
      cfg.use_psram = 1;
      _panel_instance.config_detail(cfg);
    }

    {
      // Hintergrundbeleuchtung per PWM (GPIO 2, Channel 7, 44.1 kHz)
      auto cfg = _light_instance.config();
      cfg.pin_bl      = 2;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 7;
      _light_instance.config(cfg);
      _panel_instance.light(&_light_instance);
    }

    {
      // GT911 Kapazitiver Touch (I2C SDA=19, SCL=20, Addr=0x5D)
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

  // Kompatibilitaetsmethoden fuer Bruce
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

  void setTextSize(uint8_t s) {
    _cur_textsize = s ? s : 1;
    lgfx::LGFX_Device::setTextSize(_cur_textsize);
  }
  uint8_t getTextSize(void) const { return _cur_textsize; }

  void setTextDatum(uint8_t d) {
    _cur_textdatum = d;
    lgfx::LGFX_Device::setTextDatum(d);
  }
  uint8_t getTextDatum(void) const { return _cur_textdatum; }

  void setRotation(uint8_t r) {
    _cur_rotation = r;
    lgfx::LGFX_Device::setRotation(r);
  }
  uint8_t getRotation(void) const { return _cur_rotation; }

  int16_t fontHeight(int16_t font = 1) {
    (void)font;
    return lgfx::LGFX_Device::fontHeight();
  }
  int16_t fontHeight(int16_t font = 1) const {
    (void)font;
    return const_cast<LGFX_CrowPanel70*>(this)->lgfx::LGFX_Device::fontHeight();
  }

  int16_t textWidth(const String& s, uint8_t font = 1) {
    (void)font;
    return lgfx::LGFX_Device::textWidth(s.c_str());
  }
  int16_t textWidth(const String& s, uint8_t font = 1) const {
    (void)font;
    return const_cast<LGFX_CrowPanel70*>(this)->lgfx::LGFX_Device::textWidth(s.c_str());
  }

  int16_t textWidth(const char* s, uint8_t font = 1) {
    (void)font;
    return lgfx::LGFX_Device::textWidth(s);
  }
  int16_t textWidth(const char* s, uint8_t font = 1) const {
    (void)font;
    return const_cast<LGFX_CrowPanel70*>(this)->lgfx::LGFX_Device::textWidth(s);
  }

  bool getSwapBytes(void) const { return _swapBytes; }
  void setSwapBytes(bool swap) {
    _swapBytes = swap;
    lgfx::LGFX_Device::setSwapBytes(swap);
  }

  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) const {
    return lgfx::LGFX_Device::color565(r, g, b);
  }

  void writecommand(uint8_t) {}
  void setSleepMode(bool) {}
  void imageToBin(uint8_t, const String&, int, int, bool, int) {}

  void drawArc(
      int32_t x, int32_t y, int32_t r, int32_t ir, uint32_t startAngle, uint32_t endAngle, uint32_t fg_color,
      uint32_t bg_color = 0, bool smoothArc = true
  ) {
      (void)bg_color;
      (void)smoothArc;
      lgfx::LGFX_Device::fillArc(x, y, r, ir, startAngle + 90, endAngle + 90, (uint16_t)fg_color);
  }

  void drawWideLine(float x0, float y0, float x1, float y1, float wd, uint32_t fg_color, uint32_t bg_color = 0) {
    int w = (int)(wd + 0.5f);
    if (w <= 1) {
      drawLine((int32_t)x0, (int32_t)y0, (int32_t)x1, (int32_t)y1, fg_color);
    } else {
      for (int i = -w / 2; i <= w / 2; ++i) {
        drawLine((int32_t)(x0 + i), (int32_t)y0, (int32_t)(x1 + i), (int32_t)y1, fg_color);
        drawLine((int32_t)x0, (int32_t)(y0 + i), (int32_t)x1, (int32_t)(y1 + i), fg_color);
      }
    }
  }

  int32_t drawRightString(const String& str, int32_t dX, int32_t dY, uint8_t font = 1) {
    (void)font;
    return lgfx::LGFX_Device::drawRightString(str.c_str(), dX, dY);
  }
  int32_t drawRightString(const char* str, int32_t dX, int32_t dY, uint8_t font = 1) {
    (void)font;
    return lgfx::LGFX_Device::drawRightString(str, dX, dY);
  }

  int32_t drawCentreString(const String& str, int32_t dX, int32_t dY, uint8_t font = 1) {
    (void)font;
    return lgfx::LGFX_Device::drawCenterString(str.c_str(), dX, dY);
  }
  int32_t drawCentreString(const char* str, int32_t dX, int32_t dY, uint8_t font = 1) {
    (void)font;
    return lgfx::LGFX_Device::drawCenterString(str, dX, dY);
  }

  int32_t drawString(const String& str, int32_t dX, int32_t dY, uint8_t font = 1) {
    (void)font;
    return lgfx::LGFX_Device::drawString(str.c_str(), dX, dY);
  }
  int32_t drawString(const char* str, int32_t dX, int32_t dY, uint8_t font = 1) {
    (void)font;
    return lgfx::LGFX_Device::drawString(str, dX, dY);
  }

  void fillRectHGradient(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color1, uint32_t color2) {
    (void)color2;
    lgfx::LGFX_Device::fillRect(x, y, w, h, (uint16_t)color1);
  }
  void fillRectVGradient(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color1, uint32_t color2) {
    (void)color2;
    lgfx::LGFX_Device::fillRect(x, y, w, h, (uint16_t)color1);
  }

  using lgfx::LGFX_Device::pushImage;

  void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *data) {
    lgfx::LGFX_Device::pushImage(x, y, w, h, data);
  }
  void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data) {
    lgfx::LGFX_Device::pushImage(x, y, w, h, (const uint16_t *)data);
  }
  void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint8_t *data, bool bpp8, const uint16_t *cmap) {
    if (!data || !bpp8 || !cmap) return;
    for (int32_t row = 0; row < h; ++row) {
      for (int32_t col = 0; col < w; ++col) {
        lgfx::LGFX_Device::drawPixel(x + col, y + row, cmap[data[row * w + col]]);
      }
    }
  }
  void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *data, bool bpp8, uint16_t *cmap) {
    pushImage(x, y, w, h, (const uint8_t *)data, bpp8, (const uint16_t *)cmap);
  }

  void setBrightness(uint8_t b) {
    _light_instance.setBrightness(b);
  }

  SPIClass &getSPIinstance() const {
    static SPIClass dummySPI(FSPI);
    return dummySPI;
  }
};

using tft_display = LGFX_CrowPanel70;

class CrowPanel_Sprite : public lgfx::LGFX_Sprite {
public:
  using lgfx::LGFX_Sprite::LGFX_Sprite;

  explicit CrowPanel_Sprite(LGFX_CrowPanel70 *parent) : lgfx::LGFX_Sprite(parent) {}

  void *createSprite(int16_t w, int16_t h, uint8_t frames = 1) {
    (void)frames;
    return lgfx::LGFX_Sprite::createSprite(w, h);
  }
  void deleteSprite() {
    lgfx::LGFX_Sprite::deleteSprite();
  }
  void setColorDepth(uint8_t depth) {
    lgfx::LGFX_Sprite::setColorDepth(depth);
  }
  void pushSprite(int32_t x, int32_t y, uint32_t transparent = 0x00FFFFFF) {
    lgfx::LGFX_Sprite::pushSprite(x, y, (uint16_t)transparent);
  }
  void pushToSprite(CrowPanel_Sprite *dest, int32_t x, int32_t y, uint32_t transparent = 0x00FFFFFF) {
    lgfx::LGFX_Sprite::pushSprite(static_cast<lgfx::LGFX_Sprite *>(dest), x, y, (uint16_t)transparent);
  }
  using lgfx::LGFX_Sprite::pushImage;

  void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *data) {
    lgfx::LGFX_Sprite::pushImage(x, y, w, h, data);
  }
  void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data) {
    lgfx::LGFX_Sprite::pushImage(x, y, w, h, (const uint16_t *)data);
  }
  void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint8_t *data, bool bpp8, const uint16_t *cmap) {
    if (!data || !bpp8 || !cmap) return;
    for (int32_t row = 0; row < h; ++row) {
      for (int32_t col = 0; col < w; ++col) {
        lgfx::LGFX_Sprite::drawPixel(x + col, y + row, cmap[data[row * w + col]]);
      }
    }
  }
  void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *data, bool bpp8, uint16_t *cmap) {
    pushImage(x, y, w, h, (const uint8_t *)data, bpp8, (const uint16_t *)cmap);
  }
  void fillRectHGradient(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color1, uint32_t color2) {
    (void)color2;
    lgfx::LGFX_Sprite::fillRect(x, y, w, h, (uint16_t)color1);
  }
  void fillRectVGradient(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color1, uint32_t color2) {
    (void)color2;
    lgfx::LGFX_Sprite::fillRect(x, y, w, h, (uint16_t)color1);
  }
  void drawArc(
      int32_t x, int32_t y, int32_t r, int32_t ir, uint32_t startAngle, uint32_t endAngle, uint32_t fg_color,
      uint32_t bg_color = 0, bool smoothArc = true
  ) {
      (void)bg_color;
      (void)smoothArc;
      lgfx::LGFX_Sprite::fillArc(x, y, r, ir, startAngle + 90, endAngle + 90, (uint16_t)fg_color);
  }
  void drawWideLine(float x0, float y0, float x1, float y1, float wd, uint32_t fg_color, uint32_t bg_color = 0) {
    (void)bg_color;
    lgfx::LGFX_Sprite::drawWideLine(x0, y0, x1, y1, wd, (uint16_t)fg_color);
  }
};

#ifndef MAX_LOG_ENTRIES
#define MAX_LOG_ENTRIES 64
#endif
#ifndef MAX_LOG_SIZE
#define MAX_LOG_SIZE 128
#endif
#ifndef MAX_LOG_IMAGES
#define MAX_LOG_IMAGES 1
#endif
#ifndef MAX_LOG_IMG_PATH
#define MAX_LOG_IMG_PATH 512
#endif

#ifndef __TFT_FUNCS_DEFINED__
#define __TFT_FUNCS_DEFINED__
enum tftFuncs : uint8_t {
    FILLSCREEN,           // 0
    DRAWRECT,             // 1
    FILLRECT,             // 2
    DRAWROUNDRECT,        // 3
    FILLROUNDRECT,        // 4
    DRAWCIRCLE,           // 5
    FILLCIRCLE,           // 6
    DRAWTRIAGLE,          // 7
    FILLTRIANGLE,         // 8
    DRAWELIPSE,           // 9
    FILLELIPSE,           // 10
    DRAWLINE,             // 11
    DRAWARC,              // 12
    DRAWWIDELINE,         // 13
    DRAWCENTRESTRING,     // 14
    DRAWRIGHTSTRING,      // 15
    DRAWSTRING,           // 16
    PRINT,                // 17
    DRAWIMAGE,            // 18
    DRAWPIXEL,            // 19
    DRAWFASTVLINE,        // 20
    DRAWFASTHLINE,        // 21
    SCREEN_INFO = 99      // 99
};
#endif

class tft_logger : public LGFX_CrowPanel70 {
public:
  using LGFX_CrowPanel70::LGFX_CrowPanel70;
  void setLogging(bool _log = true) { (void)_log; }
  bool getLogging(void) const { return false; }
  void setSleepMode(bool mode) { (void)mode; }
  void getBinLog(uint8_t *outBuffer, size_t &outSize) { (void)outBuffer; outSize = 0; }
  void clearLog() {}
  void addLogEntry(const uint8_t *buffer, uint8_t size) { (void)buffer; (void)size; }
  void startAsyncSerial() {}
  void stopAsyncSerial() {}
  void getTftInfo() {}
  void restoreLogger() {}
  void log_drawString(const String &s, tftFuncs fn, int32_t x, int32_t y) {
    (void)s; (void)fn; (void)x; (void)y;
  }
  void log_print(const String &s) { (void)s; }
};

using tft_sprite = CrowPanel_Sprite;
