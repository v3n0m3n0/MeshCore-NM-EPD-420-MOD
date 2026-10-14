#pragma once

#include <SPI.h>
#include <GxEPD2_BW.h>
#if __has_include(<other/GxEPD2_420_GYE042A87.h>)
#include <other/GxEPD2_420_GYE042A87.h>
#endif
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <CRC32.h>

#include <helpers/ui/DisplayDriver.h>

class NMEPD420Display : public DisplayDriver {
  static constexpr int X_OFFSET = 0;
  static constexpr int Y_OFFSET = 0;
  static constexpr uint8_t PARTIAL_REFRESH_LIMIT = 30;
  static constexpr int LOGICAL_WIDTH = 200;
  static constexpr int LOGICAL_HEIGHT = 150;

  GxEPD2_BW<EINK_DISPLAY_MODEL, EINK_DISPLAY_MODEL::HEIGHT> display;
  bool _init = false;
  bool _isOn = false;
  uint16_t _curr_color;
  CRC32 display_crc;
  uint32_t last_display_crc_value = 0;
  uint8_t partial_refresh_count = 0;

  int _font_ascender = 12;
  int _line_height = 11;
  int _cursor_x = 0;
  int _cursor_y = 0;

public:
  NMEPD420Display();

  bool begin();
  bool isOn() override { return _isOn; }
  bool isEink() override { return true; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(ColorVal bkg = UIColor::window_bkg) override;
  void setTextSize(int sz) override;
  void setColor(ColorVal c) override;
  void setCursor(int x, int y) override;
  void print(const char* str) override;
  void printWordWrap(const char* str, int max_width) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override;
  uint16_t getTextWidth(const char* str) override;
  void endFrame() override;
};
