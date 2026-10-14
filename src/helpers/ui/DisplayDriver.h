#pragma once

#include <stdint.h>
#include <string.h>

using ColorVal = uint16_t;

class UIColor {
public:
  // color definitions (by element _type_)
  static ColorVal window_bkg, title_bkg, title_txt, primary_txt, secondary_txt, warning_txt, popup_bkg, popup_txt, corp_blue;
};

class DisplayDriver {
  int _w, _h;
protected:
  DisplayDriver(int w, int h) { _w = w; _h = h; }
public:
  //enum Color { DARK=0, LIGHT, RED, GREEN, BLUE, YELLOW, ORANGE }; // on b/w screen, colors will be !=0 synonym of light

  int width() const { return _w; }
  int height() const { return _h; }

  virtual bool isOn() = 0;
  virtual bool isEink() { return false; } // default to non-eink, override in eink drivers
  virtual void turnOn() = 0;
  virtual void turnOff() = 0;
  virtual void clear() = 0;
  virtual void startFrame(ColorVal bkg = UIColor::window_bkg) = 0;
  virtual void setTextSize(int sz) = 0;
  virtual void setColor(ColorVal c) = 0;
  virtual void setCursor(int x, int y) = 0;
  virtual void print(const char* str) = 0;
  virtual void printWordWrap(const char* str, int max_width) { print(str); }   // fallback to basic print() if no override
  virtual void fillRect(int x, int y, int w, int h) = 0;
  virtual void drawRect(int x, int y, int w, int h) = 0;
  virtual void drawXbm(int x, int y, const uint8_t* bits, int w, int h) = 0;
  virtual uint16_t getTextWidth(const char* str) = 0;
  virtual void drawTextCentered(int mid_x, int y, const char* str) {   // helper method (override to optimise)
    int w = getTextWidth(str);
    setCursor(mid_x - w/2, y);
    print(str);
  }
  virtual void drawTextRightAlign(int x_anch, int y, const char* str) {
    int w = getTextWidth(str);
    setCursor(x_anch - w, y);
    print(str);
  }
  virtual void drawTextLeftAlign(int x_anch, int y, const char* str) {
    setCursor(x_anch, y);
    print(str);
  }
  
  // convert UTF-8 characters to readable ASCII characters for compatibility
  virtual void translateUTF8ToBlocks(char* dest, const char* src, size_t dest_size) {
    if (!dest || dest_size == 0) return;
    if (!src) {
      dest[0] = '\0';
      return;
    }

    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j < dest_size - 1; i++) {
      unsigned char c = (unsigned char)src[i];
      if (c < 0x80) {
        if (c >= 32 && c <= 126) {
          dest[j++] = c;
        } else if (c == '\n' || c == '\r' || c == '\t') {
          dest[j++] = c;
        }
      } else if (c == 0xC3) {
        unsigned char c2 = (unsigned char)src[++i];
        if (!c2) break;
        switch (c2) {
          case 0x93: dest[j++] = 'O'; break; // Ó
          case 0xB3: dest[j++] = 'o'; break; // ó
          case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: dest[j++] = 'A'; break; // À-Å
          case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: dest[j++] = 'a'; break; // à-å
          case 0x86: dest[j++] = 'A'; if (j < dest_size - 1) dest[j++] = 'E'; break; // Æ
          case 0xA6: dest[j++] = 'a'; if (j < dest_size - 1) dest[j++] = 'e'; break; // æ
          case 0x87: dest[j++] = 'C'; break; // Ç
          case 0xA7: dest[j++] = 'c'; break; // ç
          case 0x88: case 0x89: case 0x8A: case 0x8B: dest[j++] = 'E'; break; // È-Ë
          case 0xA8: case 0xA9: case 0xAA: case 0xAB: dest[j++] = 'e'; break; // è-ë
          case 0x8C: case 0x8D: case 0x8E: case 0x8F: dest[j++] = 'I'; break; // Ì-Ï
          case 0xAC: case 0xAD: case 0xAE: case 0xAF: dest[j++] = 'i'; break; // ì-ï
          case 0x91: dest[j++] = 'N'; break; // Ñ
          case 0xB1: dest[j++] = 'n'; break; // ñ
          case 0x92: case 0x94: case 0x95: case 0x96: case 0x98: dest[j++] = 'O'; break; // Ò,Ô,Õ,Ö,Ø
          case 0xB2: case 0xB4: case 0xB5: case 0xB6: case 0xB8: dest[j++] = 'o'; break; // ò,ô,õ,ö,ø
          case 0x99: case 0x9A: case 0x9B: case 0x9C: dest[j++] = 'U'; break; // Ù-Ü
          case 0xB9: case 0xBA: case 0xBB: case 0xBC: dest[j++] = 'u'; break; // ù-ü
          case 0x9D: dest[j++] = 'Y'; break; // Ý
          case 0xBD: case 0xBF: dest[j++] = 'y'; break; // ý,ÿ
          case 0x9F: dest[j++] = 's'; break; // ß
          default: dest[j++] = '?'; break;
        }
      } else if (c == 0xC4) {
        unsigned char c2 = (unsigned char)src[++i];
        if (!c2) break;
        switch (c2) {
          case 0x84: dest[j++] = 'A'; break; // Ą
          case 0x85: dest[j++] = 'a'; break; // ą
          case 0x86: dest[j++] = 'C'; break; // Ć
          case 0x87: dest[j++] = 'c'; break; // ć
          case 0x8C: dest[j++] = 'C'; break; // Č
          case 0x8D: dest[j++] = 'c'; break; // č
          case 0x8E: dest[j++] = 'D'; break; // Ď
          case 0x8F: dest[j++] = 'd'; break; // ď
          case 0x98: dest[j++] = 'E'; break; // Ę
          case 0x99: dest[j++] = 'e'; break; // ę
          case 0x9A: dest[j++] = 'E'; break; // Ě
          case 0x9B: dest[j++] = 'e'; break; // ě
          default: dest[j++] = '?'; break;
        }
      } else if (c == 0xC5) {
        unsigned char c2 = (unsigned char)src[++i];
        if (!c2) break;
        switch (c2) {
          case 0x81: dest[j++] = 'L'; break; // Ł
          case 0x82: dest[j++] = 'l'; break; // ł
          case 0x83: dest[j++] = 'N'; break; // Ń
          case 0x84: dest[j++] = 'n'; break; // ń
          case 0x87: dest[j++] = 'N'; break; // Ň
          case 0x88: dest[j++] = 'n'; break; // ň
          case 0x98: dest[j++] = 'R'; break; // Ř
          case 0x99: dest[j++] = 'r'; break; // ř
          case 0x9A: dest[j++] = 'S'; break; // Ś
          case 0x9B: dest[j++] = 's'; break; // ś
          case 0xA0: dest[j++] = 'S'; break; // Š
          case 0xA1: dest[j++] = 's'; break; // š
          case 0xA4: dest[j++] = 'T'; break; // Ť
          case 0xA5: dest[j++] = 't'; break; // ť
          case 0xB9: dest[j++] = 'Z'; break; // Ź
          case 0xBA: dest[j++] = 'z'; break; // ź
          case 0xBB: dest[j++] = 'Z'; break; // Ż
          case 0xBC: dest[j++] = 'z'; break; // ż
          case 0xBD: dest[j++] = 'Z'; break; // Ž
          case 0xBE: dest[j++] = 'z'; break; // ž
          default: dest[j++] = '?'; break;
        }
      } else if (c == 0xE2) {
        unsigned char c2 = (unsigned char)src[++i];
        unsigned char c3 = c2 ? (unsigned char)src[++i] : 0;
        if (c2 == 0x80) {
          switch (c3) {
            case 0x98: case 0x99: case 0x9A: dest[j++] = '\''; break; // ‘ ’ ‚
            case 0x9C: case 0x9D: case 0x9E: dest[j++] = '"'; break;  // “ ” „
            case 0x93: case 0x94: dest[j++] = '-'; break;             // – —
            case 0xA6: dest[j++] = '.'; break;                        // …
            default: dest[j++] = ' '; break;
          }
        } else {
          dest[j++] = ' ';
        }
      } else {
        dest[j++] = '?';
        while (src[i+1] && ((unsigned char)src[i+1] & 0xC0) == 0x80) {
          i++;
        }
      }
    }
    dest[j] = '\0';
  }
  
  // draw text with ellipsis if it exceeds max_width
  virtual void drawTextEllipsized(int x, int y, int max_width, const char* str) {
    char temp_str[256];  // reasonable buffer size
    size_t len = strlen(str);
    if (len >= sizeof(temp_str)) len = sizeof(temp_str) - 1;
    memcpy(temp_str, str, len);
    temp_str[len] = 0;
    
    if (getTextWidth(temp_str) <= max_width) {
      setCursor(x, y);
      print(temp_str);
      return;
    }
    
    // for variable-width fonts (GxEPD), add space after ellipsis
    // for fixed-width fonts (OLED), keep tight spacing to save precious characters
    const char* ellipsis;
    // use a simple heuristic: if 'i' and 'l' have different widths, it's variable-width
    int i_width = getTextWidth("i");
    int l_width = getTextWidth("l");
    if (i_width != l_width) {
      ellipsis = "... ";  // variable-width fonts: add space
    } else {
      ellipsis = "...";   // fixed-width fonts: no space
    }
    
    int ellipsis_width = getTextWidth(ellipsis);
    int str_len = strlen(temp_str);
    
    while (str_len > 0 && getTextWidth(temp_str) > max_width - ellipsis_width) {
      temp_str[--str_len] = 0;
    }
    strcat(temp_str, ellipsis);
    
    setCursor(x, y);
    print(temp_str);
  }
  
  virtual void endFrame() = 0;
};
