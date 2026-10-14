#ifdef DISPLAY_CLASS

#include "NMEPD420Display.h"

#include <math.h>

static SPIClass nm_epd_spi(FSPI);

NMEPD420Display::NMEPD420Display()
  : DisplayDriver(128, 128),
    display(EINK_DISPLAY_MODEL(PIN_DISPLAY_CS, PIN_DISPLAY_DC, PIN_DISPLAY_RST, PIN_DISPLAY_BUSY)) {}

bool NMEPD420Display::begin() {
  pinMode(PIN_DISPLAY_CS, OUTPUT);
  digitalWrite(PIN_DISPLAY_CS, HIGH);
  pinMode(PIN_DISPLAY_DC, OUTPUT);
  pinMode(PIN_DISPLAY_BUSY, INPUT);
  pinMode(PIN_DISPLAY_RST, OUTPUT);
  digitalWrite(PIN_DISPLAY_RST, HIGH);

  display.epd2.selectSPI(nm_epd_spi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  nm_epd_spi.begin(PIN_DISPLAY_SCLK, PIN_DISPLAY_MISO, PIN_DISPLAY_MOSI, PIN_DISPLAY_CS);
  display.init(115200, true, 2, false);
  display.setRotation(DISPLAY_ROTATION);
  setTextSize(1);
  display.setFullWindow();
  display.fillScreen(GxEPD_WHITE);
  display.display(false);
  display.setPartialWindow(0, 0, display.width(), display.height());
  _init = true;
  return true;
}

void NMEPD420Display::turnOn() {
  if (!_init) begin();
  _isOn = true;
}

void NMEPD420Display::turnOff() {
  _isOn = false;
}

void NMEPD420Display::clear() {
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display_crc.reset();
  _cursor_x = X_OFFSET * EINK_SCALE_X;
  _cursor_y = Y_OFFSET * EINK_SCALE_Y;
  display.setCursor(_cursor_x, _cursor_y + _font_ascender);
}

void NMEPD420Display::startFrame(ColorVal bkg) {
  display.fillScreen(bkg);
  display.setTextColor(_curr_color = UIColor::primary_txt);
  display_crc.reset();
  _cursor_x = X_OFFSET * EINK_SCALE_X;
  _cursor_y = Y_OFFSET * EINK_SCALE_Y;
  display.setCursor(_cursor_x, _cursor_y + _font_ascender);
}

void NMEPD420Display::setTextSize(int sz) {
  display_crc.update<int>(sz);
  switch (sz) {
    case 1:
      display.setFont(&FreeSans9pt7b);
      _font_ascender = 12;
      _line_height = 11;
      break;
    case 2:
      display.setFont(&FreeSansBold12pt7b);
      _font_ascender = 17;
      _line_height = 16;
      break;
    case 3:
      display.setFont(&FreeSans18pt7b);
      _font_ascender = 25;
      _line_height = 24;
      break;
    default:
      display.setFont(&FreeSans9pt7b);
      _font_ascender = 12;
      _line_height = 11;
      break;
  }
  display.setCursor(_cursor_x, _cursor_y + _font_ascender);
}

void NMEPD420Display::setColor(ColorVal c) {
  display_crc.update<ColorVal>(c);
  display.setTextColor(_curr_color = c);
}

void NMEPD420Display::setCursor(int x, int y) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  _cursor_x = (x + X_OFFSET) * EINK_SCALE_X;
  _cursor_y = (y + Y_OFFSET) * EINK_SCALE_Y;
  display.setCursor(_cursor_x, _cursor_y + _font_ascender);
}

void NMEPD420Display::print(const char* str) {
  if (!str) return;
  bool has_utf8 = false;
  for (const char* p = str; *p; p++) {
    if ((unsigned char)*p >= 0x80) { has_utf8 = true; break; }
  }
  if (has_utf8) {
    char buf[512];
    translateUTF8ToBlocks(buf, str, sizeof(buf));
    display_crc.update<char>(buf, strlen(buf));
    display.print(buf);
  } else {
    display_crc.update<char>(str, strlen(str));
    display.print(str);
  }
}

void NMEPD420Display::printWordWrap(const char* str, int max_width) {
  if (!str || !*str) return;

  char buf[512];
  bool has_utf8 = false;
  for (const char* p = str; *p; p++) {
    if ((unsigned char)*p >= 0x80) { has_utf8 = true; break; }
  }
  if (has_utf8) {
    translateUTF8ToBlocks(buf, str, sizeof(buf));
    str = buf;
  }

  display_crc.update<char>(str, strlen(str));

  const int max_w_native = max_width * EINK_SCALE_X;
  const int start_x = _cursor_x;
  int cur_x = _cursor_x;
  int cur_y = _cursor_y;
  const int line_step = _line_height * EINK_SCALE_Y;

  // Measure space width using difference between "x x" and "xx"
  int16_t x1, y1;
  uint16_t w_sp, h_sp, w_xx;
  display.getTextBounds("x x", 0, 0, &x1, &y1, &w_sp, &h_sp);
  display.getTextBounds("xx", 0, 0, &x1, &y1, &w_xx, &h_sp);
  const int space_advance = (w_sp > w_xx) ? (w_sp - w_xx) : 6;

  const char* p = str;
  while (*p) {
    if (*p == '\r') {
      p++;
      continue;
    }
    if (*p == '\n') {
      cur_x = start_x;
      cur_y += line_step;
      display.setCursor(cur_x, cur_y + _font_ascender);
      p++;
      continue;
    }

    if (*p == ' ') {
      if (cur_x > start_x && (cur_x + space_advance <= start_x + max_w_native)) {
        display.print(" ");
        cur_x = display.getCursorX();
      }
      p++;
      continue;
    }

    char word[64];
    size_t wlen = 0;
    while (*p && *p != ' ' && *p != '\n' && *p != '\r' && wlen < sizeof(word) - 1) {
      word[wlen++] = *p++;
    }
    word[wlen] = '\0';

    int16_t wx, wy;
    uint16_t ww, wh;
    display.getTextBounds(word, 0, 0, &wx, &wy, &ww, &wh);

    if (cur_x > start_x && (cur_x + ww) > (start_x + max_w_native)) {
      cur_x = start_x;
      cur_y += line_step;
      display.setCursor(cur_x, cur_y + _font_ascender);
    }

    display.print(word);
    cur_x = display.getCursorX();
  }

  _cursor_x = cur_x;
  _cursor_y = cur_y;
}

void NMEPD420Display::fillRect(int x, int y, int w, int h) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
  display.fillRect((x + X_OFFSET) * EINK_SCALE_X, (y + Y_OFFSET) * EINK_SCALE_Y,
                   w * EINK_SCALE_X, h * EINK_SCALE_Y, _curr_color);
}

void NMEPD420Display::drawRect(int x, int y, int w, int h) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
  display.drawRect((x + X_OFFSET) * EINK_SCALE_X, (y + Y_OFFSET) * EINK_SCALE_Y,
                   w * EINK_SCALE_X, h * EINK_SCALE_Y, _curr_color);
}

void NMEPD420Display::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
  display_crc.update<uint8_t>(bits, w * h / 8);

  const uint16_t start_x = (x + X_OFFSET) * EINK_SCALE_X;
  const uint16_t start_y = (y + Y_OFFSET) * EINK_SCALE_Y;
  const uint16_t width_in_bytes = (w + 7) / 8;
  for (uint16_t by = 0; by < h; by++) {
    const int y1 = start_y + static_cast<int>(by * EINK_SCALE_Y);
    const int y2 = start_y + static_cast<int>((by + 1) * EINK_SCALE_Y);
    for (uint16_t bx = 0; bx < w; bx++) {
      const int x1 = start_x + static_cast<int>(bx * EINK_SCALE_X);
      const int x2 = start_x + static_cast<int>((bx + 1) * EINK_SCALE_X);
      const uint16_t byte_offset = by * width_in_bytes + bx / 8;
      const uint8_t bit_mask = 0x80 >> (bx & 7);
      if (pgm_read_byte(bits + byte_offset) & bit_mask) {
        display.fillRect(x1, y1, x2 - x1, y2 - y1, _curr_color);
      }
    }
  }
}

uint16_t NMEPD420Display::getTextWidth(const char* str) {
  if (!str || !*str) return 0;
  char buf[256];
  bool has_utf8 = false;
  for (const char* p = str; *p; p++) {
    if ((unsigned char)*p >= 0x80) { has_utf8 = true; break; }
  }
  if (has_utf8) {
    translateUTF8ToBlocks(buf, str, sizeof(buf));
    str = buf;
  }
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  return ceil((w + 1) / EINK_SCALE_X);
}

void NMEPD420Display::endFrame() {
  uint32_t crc = display_crc.finalize();
  if (crc == last_display_crc_value) return;

  if (partial_refresh_count >= PARTIAL_REFRESH_LIMIT) {
    display.setFullWindow();
    display.display(false);
    partial_refresh_count = 0;
    display.setPartialWindow(0, 0, display.width(), display.height());
  } else {
    display.display(true);
    partial_refresh_count++;
  }

  last_display_crc_value = crc;
}

#endif
