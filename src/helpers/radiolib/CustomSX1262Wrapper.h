#pragma once

#include "CustomSX1262.h"
#include "RadioLibWrappers.h"
#include "SX126xReset.h"

#ifndef USE_SX1262
#define USE_SX1262
#endif

class CustomSX1262Wrapper : public RadioLibWrapper {
public:
  CustomSX1262Wrapper(CustomSX1262& radio, mesh::MainBoard& board) : RadioLibWrapper(radio, board) { }

  void setParams(float freq, float bw, uint8_t sf, uint8_t cr) override {
    const bool was_rx = isInRecvMode();
    idle();
    auto* sx = static_cast<CustomSX1262*>(_radio);
    sx->standby();
    sx->setFrequency(freq);
    sx->setSpreadingFactor(sf);
    sx->setBandwidth(bw);
    sx->setCodingRate(cr);
    sx->setRxBoostedGainMode(sx->getRxBoostedGainMode());
    updatePreamble(sf);
    PacketMillis pm = calcMaxPacketMillis(sf, bw, cr, preambleLengthForSF(sf));
    sx->setPreambleMillis(pm.preambleMillis);
    sx->setMaxPayloadMillis(pm.payloadMillis);
    if (was_rx) {
      startRecv();
    }
  }

  bool isReceivingPacket() override { 
    return ((CustomSX1262 *)_radio)->isReceiving();
  }
  float getCurrentRSSI() override {
    return ((CustomSX1262 *)_radio)->getRSSI(false);
  }
  float getLastRSSI() const override { return ((CustomSX1262 *)_radio)->getRSSI(); }
  float getLastSNR() const override { return ((CustomSX1262 *)_radio)->getSNR(); }

  float packetScore(float snr, int packet_len) override {
    int sf = ((CustomSX1262 *)_radio)->spreadingFactor;
    return packetScoreInt(snr, sf, packet_len);
  }
  uint8_t getSpreadingFactor() const override { return ((CustomSX1262 *)_radio)->spreadingFactor; }
  virtual void powerOff() override {
    ((CustomSX1262 *)_radio)->sleep(false);
  }

  bool setRxBoostedGainMode(bool en) override {
    return ((CustomSX1262 *)_radio)->setRxBoostedGainMode(en) == RADIOLIB_ERR_NONE;
  }
  bool getRxBoostedGainMode() const override {
    return ((CustomSX1262 *)_radio)->getRxBoostedGainMode();
  }

  void doResetAGC() override { sx126xResetAGC((SX126x *)_radio, getRxBoostedGainMode()); }
};
