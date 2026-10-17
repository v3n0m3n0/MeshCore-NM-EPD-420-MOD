#ifdef DISPLAY_CLASS

#include "NMEPD420Display.h"

#include <math.h>

static SPIClass nm_epd_spi(FSPI);

NMEPD420Display::NMEPD420Display()
  : DisplayDriver(LOGICAL_WIDTH, LOGICAL_HEIGHT),
    display(EINK_DISPLAY_MODEL(PIN_DISPLAY_CS, PIN_DISPLAY_DC, PIN_DISPLAY_RST, PIN_DISPLAY_BUSY)) {}

void NMEPD420Display::epdRefreshTask(void* param) {
  auto* self = static_cast<NMEPD420Display*>(param);
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (self->_epd_mutex && xSemaphoreTake(self->_epd_mutex, portMAX_DELAY) == pdTRUE) {
      if (self->_refresh_full) {
        self->display.setFullWindow();
        self->display.display(false);
        self->display.setPartialWindow(0, 0, self->display.width(), self->display.height());
      } else {
        self->display.display(true);
      }
      // Power off SSD1683 charge pumps immediately after refresh to eliminate idle current draw
      self->display.powerOff();
      self->_is_refreshing = false;
      xSemaphoreGive(self->_epd_mutex);
    } else {
      self->_is_refreshing = false;
    }
  }
}

void NMEPD420Display::waitWhileBusy() {
  if (_epd_mutex) {
    if (xSemaphoreTake(_epd_mutex, portMAX_DELAY) == pdTRUE) {
      xSemaphoreGive(_epd_mutex);
    }
  }
}

bool NMEPD420Display::begin() {
  pinMode(PIN_DISPLAY_CS, OUTPUT);
  digitalWrite(PIN_DISPLAY_CS, HIGH);
  pinMode(PIN_DISPLAY_DC, OUTPUT);
  pinMode(PIN_DISPLAY_BUSY, INPUT);
  pinMode(PIN_DISPLAY_RST, OUTPUT);
  digitalWrite(PIN_DISPLAY_RST, HIGH);

  if (!_epd_mutex) {
    _epd_mutex = xSemaphoreCreateMutex();
  }
  if (!_refresh_task_handle) {
    xTaskCreatePinnedToCore(epdRefreshTask, "epd_ref", 4096, this, 1, &_refresh_task_handle, 0);
  }

  // SSD1683 supports up to 20 MHz SPI write clock; 16 MHz achieves 4x faster frame transfer than 4 MHz
  display.epd2.selectSPI(nm_epd_spi, SPISettings(16000000, MSBFIRST, SPI_MODE0));
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
  if (_isOn) {
    waitWhileBusy();
    display.powerOff();
    _isOn = false;
  }
}

void NMEPD420Display::clear() {
  waitWhileBusy();
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display_crc.reset();
  _cursor_x = X_OFFSET * EINK_SCALE_X;
  _cursor_y = Y_OFFSET * EINK_SCALE_Y;
  display.setCursor(_cursor_x, _cursor_y + _font_ascender);
}

void NMEPD420Display::startFrame(ColorVal bkg) {
  waitWhileBusy();
  display.fillScreen(bkg);
  display.setTextColor(_curr_color = UIColor::primary_txt);
  display_crc.reset();
  display_crc.update<ColorVal>(bkg);
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
      _space_advance = 6;
      break;
    case 2:
      display.setFont(&FreeSansBold12pt7b);
      _font_ascender = 17;
      _line_height = 16;
      _space_advance = 8;
      break;
    case 3:
      display.setFont(&FreeSans18pt7b);
      _font_ascender = 25;
      _line_height = 24;
      _space_advance = 11;
      break;
    default:
      display.setFont(&FreeSans9pt7b);
      _font_ascender = 12;
      _line_height = 11;
      _space_advance = 6;
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

  // Use cached font space advance
  const int space_advance = _space_advance;

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
    int span_start = -1;
    for (uint16_t bx = 0; bx < w; bx++) {
      const uint16_t byte_offset = by * width_in_bytes + bx / 8;
      const uint8_t bit_mask = 0x80 >> (bx & 7);
      bool is_set = (pgm_read_byte(bits + byte_offset) & bit_mask) != 0;
      if (is_set) {
        if (span_start < 0) span_start = bx;
      } else {
        if (span_start >= 0) {
          const int x1 = start_x + static_cast<int>(span_start * EINK_SCALE_X);
          const int x2 = start_x + static_cast<int>(bx * EINK_SCALE_X);
          display.fillRect(x1, y1, x2 - x1, y2 - y1, _curr_color);
          span_start = -1;
        }
      }
    }
    if (span_start >= 0) {
      const int x1 = start_x + static_cast<int>(span_start * EINK_SCALE_X);
      const int x2 = start_x + static_cast<int>(w * EINK_SCALE_X);
      display.fillRect(x1, y1, x2 - x1, y2 - y1, _curr_color);
    }
  }
}

uint16_t NMEPD420Display::getTextWidth(const char* str) {
  if (!str || !*str) return 0;
  char buf[512];
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
    _refresh_full = true;
    partial_refresh_count = 0;
  } else {
    _refresh_full = false;
    partial_refresh_count++;
  }

  last_display_crc_value = crc;

  if (_refresh_task_handle) {
    _is_refreshing = true;
    xTaskNotifyGive(_refresh_task_handle);
  } else {
    if (_refresh_full) {
      display.setFullWindow();
      display.display(false);
      display.setPartialWindow(0, 0, display.width(), display.height());
    } else {
      display.display(true);
    }
    display.powerOff();
  }
}

void NMEPD420Display::forceFullRefresh() {
  partial_refresh_count = PARTIAL_REFRESH_LIMIT;
  last_display_crc_value = 0;
}

#endif
