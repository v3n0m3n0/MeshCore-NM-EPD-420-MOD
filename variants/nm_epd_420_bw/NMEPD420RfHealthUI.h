#pragma once

#ifdef DISPLAY_CLASS

#include <Arduino.h>
#include <stdio.h>
#include <helpers/ui/DisplayDriver.h>
#include <NMEPD420Board.h>
#include <NMEPD420SDLogger.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>

extern NMEPD420Board board;
extern CustomSX1262Wrapper radio_driver;
extern NMEPD420SDLogger sd_logger;

inline void nmRenderRfHealthView(DisplayDriver& display,
                                 float freq_mhz,
                                 uint8_t sf,
                                 float airtime_factor,
                                 unsigned long tx_air_ms,
                                 unsigned long rx_air_ms,
                                 unsigned long tx_budget_ms,
                                 uint16_t batt_mv,
                                 int top_y,
                                 const char* footer_text) {
  char sd_buf[32];
  char val[32];
  display.setTextSize(1);

  display.setColor(UIColor::corp_blue);
  display.drawTextLeftAlign(0, top_y, "RF HEALTH");
  display.setColor(UIColor::primary_txt);
  snprintf(val, sizeof(val), "%.3fM SF%u", freq_mhz, sf);
  display.drawTextRightAlign(display.width() - 1, top_y, val);

  display.setColor(UIColor::secondary_txt);
  display.drawRect(0, top_y + 12, display.width(), 1);

  // Quantize noise floor in 3 dBm steps to avoid partial-refresh flicker while idle.
  const int q_floor = (radio_driver.getNoiseFloor() / 3) * 3;
  const int last_rssi = static_cast<int>(radio_driver.getLastRSSI());
  const float last_snr = radio_driver.getLastSNR();
  display.setColor(UIColor::secondary_txt);
  display.drawTextLeftAlign(0, top_y + 17, "Noise / Last:");
  display.setColor(UIColor::primary_txt);
  snprintf(val, sizeof(val), "%ddBm %d/%.1f", q_floor, last_rssi, last_snr);
  display.drawTextRightAlign(display.width() - 1, top_y + 17, val);

  display.setColor(UIColor::secondary_txt);
  display.drawTextLeftAlign(0, top_y + 32, "RX / CRC / SPI:");
  display.setColor(UIColor::primary_txt);
  snprintf(val, sizeof(val), "%lu / %lu / %lu",
           static_cast<unsigned long>(radio_driver.getPacketsRecv()),
           static_cast<unsigned long>(radio_driver.getPacketsCrcErrors()),
           static_cast<unsigned long>(radio_driver.getRadioSpiErrors()));
  display.drawTextRightAlign(display.width() - 1, top_y + 32, val);

  display.setColor(UIColor::secondary_txt);
  display.drawTextLeftAlign(0, top_y + 47, "Airtime TX/RX:");
  display.setColor(UIColor::primary_txt);
  snprintf(val, sizeof(val), "%lus / %lus",
           tx_air_ms / 1000UL,
           rx_air_ms / 1000UL);
  display.drawTextRightAlign(display.width() - 1, top_y + 47, val);

  display.setColor(UIColor::secondary_txt);
  display.drawTextLeftAlign(0, top_y + 62, "Duty Budget:");
  display.setColor(UIColor::primary_txt);
  snprintf(val, sizeof(val), "%lus (af:%.1f)",
           tx_budget_ms / 1000UL,
           airtime_factor);
  display.drawTextRightAlign(display.width() - 1, top_y + 62, val);

  display.setColor(UIColor::secondary_txt);
  display.drawTextLeftAlign(0, top_y + 77, "Power / ADC:");
  display.setColor(UIColor::primary_txt);
  snprintf(val, sizeof(val), "%umV x%.2f %s",
           static_cast<unsigned>(batt_mv),
           board.getAdcMultiplier(),
           board.isUsbPowered() ? "USB" : "BAT");
  display.drawTextRightAlign(display.width() - 1, top_y + 77, val);

  sd_logger.formatUiSummary(sd_buf, sizeof(sd_buf));
  display.setColor(UIColor::secondary_txt);
  display.drawTextLeftAlign(0, top_y + 92, "microSD:");
  display.setColor(UIColor::primary_txt);
  display.drawTextRightAlign(display.width() - 1, top_y + 92, sd_buf);

  if (footer_text && footer_text[0]) {
    display.setColor(UIColor::secondary_txt);
    display.drawTextCentered(display.width() / 2, top_y + 112, footer_text);
  }
}

#endif
