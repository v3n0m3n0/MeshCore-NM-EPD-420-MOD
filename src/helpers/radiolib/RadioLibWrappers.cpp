
#define RADIOLIB_STATIC_ONLY 1
#include "RadioLibWrappers.h"

#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
static portMUX_TYPE radioStateMux = portMUX_INITIALIZER_UNLOCKED;
#define RADIO_STATE_ENTER() portENTER_CRITICAL(&radioStateMux)
#define RADIO_STATE_EXIT() portEXIT_CRITICAL(&radioStateMux)
#define RADIO_STATE_ENTER_ISR() portENTER_CRITICAL_ISR(&radioStateMux)
#define RADIO_STATE_EXIT_ISR() portEXIT_CRITICAL_ISR(&radioStateMux)
#else
#define RADIO_STATE_ENTER()
#define RADIO_STATE_EXIT()
#define RADIO_STATE_ENTER_ISR()
#define RADIO_STATE_EXIT_ISR()
#endif

#define STATE_IDLE       0
#define STATE_RX         1
#define STATE_TX_WAIT    3
#define STATE_TX_DONE    4
#define STATE_INT_READY 16

#define NUM_NOISE_FLOOR_SAMPLES  64
#define SAMPLING_THRESHOLD  14

static volatile uint8_t state = STATE_IDLE;

// this function is called when a complete packet
// is transmitted by the module
static
#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  // we sent/received a packet, set the flag atomically against Core 1 state updates
  RADIO_STATE_ENTER_ISR();
  state |= STATE_INT_READY;
  RADIO_STATE_EXIT_ISR();
}

void RadioLibWrapper::begin() {
  _radio->setPacketReceivedAction(setFlag);  // this is also SentComplete interrupt
  _preamble_sf = getSpreadingFactor();
  _radio->setPreambleLength(preambleLengthForSF(_preamble_sf)); // longer preamble for lower SF improves reliability
  RADIO_STATE_ENTER();
  state = STATE_IDLE;
  RADIO_STATE_EXIT();

  if (_board->getStartupReason() == BD_STARTUP_RX_PACKET) {  // received a LoRa packet (while in deep sleep)
    setFlag(); // LoRa packet is already received
  }

  _noise_floor = 0;
  _threshold = 0;
  _cad_enabled = false;

  // start average out some samples
  _num_floor_samples = 0;
  _floor_sample_sum = 0;
}

uint32_t RadioLibWrapper::getRngSeed() {
  return _radio->random(0x7FFFFFFF);
}

void RadioLibWrapper::setTxPower(int8_t dbm) {
#if defined(USE_LR2021)
  idle();
#endif
  _radio->setOutputPower(dbm);
}

void RadioLibWrapper::idle() {
  _radio->standby();
  RADIO_STATE_ENTER();
  state = STATE_IDLE;   // need another startReceive()
  RADIO_STATE_EXIT();
}

void RadioLibWrapper::triggerNoiseFloorCalibrate(int threshold) {
  _threshold = threshold;
  if (_num_floor_samples >= NUM_NOISE_FLOOR_SAMPLES) {  // ignore trigger if currently sampling
    _num_floor_samples = 0;
    _floor_sample_sum = 0;
  }
}

void RadioLibWrapper::doResetAGC() {
  _radio->sleep();  // warm sleep to reset analog frontend
}

void RadioLibWrapper::resetAGC() {
  uint8_t cur_state;
  RADIO_STATE_ENTER();
  cur_state = state;
  RADIO_STATE_EXIT();

  // make sure we're not mid-receive of packet!
  if ((cur_state & STATE_INT_READY) != 0 || isReceivingPacket()) return;

  doResetAGC();
  RADIO_STATE_ENTER();
  state = STATE_IDLE;   // trigger a startReceive()
  RADIO_STATE_EXIT();

  // Reset noise floor sampling so it reconverges from scratch.
  // Without this, a stuck _noise_floor of -120 makes the sampling threshold
  // too low (-106) to accept normal samples (~-105), self-reinforcing the
  // stuck value even after the receiver has recovered.
  _noise_floor = 0;
  _num_floor_samples = 0;
  _floor_sample_sum = 0;
}

void RadioLibWrapper::loop() {
  uint8_t cur_state;
  RADIO_STATE_ENTER();
  cur_state = state;
  RADIO_STATE_EXIT();

  if (cur_state == STATE_RX && _num_floor_samples < NUM_NOISE_FLOOR_SAMPLES) {
    if (!isReceivingPacket()) {
      int rssi = getCurrentRSSI();
      if (rssi < _noise_floor + SAMPLING_THRESHOLD) {  // only consider samples below current floor + sampling THRESHOLD
        _num_floor_samples++;
        _floor_sample_sum += rssi;
      }
    }
  } else if (_num_floor_samples >= NUM_NOISE_FLOOR_SAMPLES && _floor_sample_sum != 0) {
    _noise_floor = _floor_sample_sum / NUM_NOISE_FLOOR_SAMPLES;
    if (_noise_floor < -120) {
      _noise_floor = -120;    // clamp to lower bound of -120dBi
    }
    _floor_sample_sum = 0;

    #ifdef MESH_DEBUG_NOISE_FLOOR
    MESH_DEBUG_PRINTLN("RadioLibWrapper: noise_floor = %d", (int)_noise_floor);
    #endif
  }
}

void RadioLibWrapper::startRecv() {
  #if defined(USE_LR2021)
  _radio->standby(); // without this LR2021 can throw -706 when calling startReceive after hardware CAD when side detectors are enabled
  #endif
  int err = _radio->startReceive();
  if (err == RADIOLIB_ERR_NONE) {
    RADIO_STATE_ENTER();
    state = (state & STATE_INT_READY) | STATE_RX;
    RADIO_STATE_EXIT();
  } else {
    n_spi_errors++;
    MESH_DEBUG_PRINTLN("RadioLibWrapper: error: startReceive(%d)", err);
  }
}

bool RadioLibWrapper::isInRecvMode() const {
  uint8_t cur_state;
  RADIO_STATE_ENTER();
  cur_state = state;
  RADIO_STATE_EXIT();
  return (cur_state & ~STATE_INT_READY) == STATE_RX;
}

int RadioLibWrapper::recvRaw(uint8_t* bytes, int sz) {
  int len = 0;
  bool int_ready = false;
  RADIO_STATE_ENTER();
  if (state & STATE_INT_READY) {
    state &= ~STATE_INT_READY;
    int_ready = true;
  }
  RADIO_STATE_EXIT();

  if (int_ready) {
    len = _radio->getPacketLength();
    if (len > 0) {
      if (len > sz) { len = sz; }
      int err = _radio->readData(bytes, len);
      if (err != RADIOLIB_ERR_NONE) {
        MESH_DEBUG_PRINTLN("RadioLibWrapper: error: readData(%d)", err);
        len = 0;
        n_recv_errors++;
        if (err == RADIOLIB_ERR_CRC_MISMATCH || err == RADIOLIB_ERR_LORA_HEADER_DAMAGED) {
          n_crc_errors++;
        } else {
          n_spi_errors++;
        }
      } else {
      //  Serial.print("  readData() -> "); Serial.println(len);
        n_recv++;
      }
    }
    RADIO_STATE_ENTER();
    #if defined(USE_LR2021)
    state = (state & STATE_INT_READY) | STATE_RX;     // LR2021 stays in Rx after readData, calling startReceive while still in Rx throws -706 errors
    #else
    state = STATE_IDLE;   // need another startReceive()
    #endif
    RADIO_STATE_EXIT();
  }

  uint8_t cur_state;
  RADIO_STATE_ENTER();
  cur_state = state;
  RADIO_STATE_EXIT();

  if ((cur_state & ~STATE_INT_READY) != STATE_RX) {
    int err = _radio->startReceive();
    if (err == RADIOLIB_ERR_NONE) {
      RADIO_STATE_ENTER();
      state = (state & STATE_INT_READY) | STATE_RX;
      RADIO_STATE_EXIT();
    } else {
      n_spi_errors++;
      MESH_DEBUG_PRINTLN("RadioLibWrapper: error: startReceive(%d)", err);
    }
  }
  return len;
}

uint32_t RadioLibWrapper::getEstAirtimeFor(int len_bytes) {
  return _radio->getTimeOnAir(len_bytes) / 1000;
}

bool RadioLibWrapper::startSendRaw(const uint8_t* bytes, int len) {
  _board->onBeforeTransmit();
  RADIO_STATE_ENTER();
  state = STATE_TX_WAIT;
  RADIO_STATE_EXIT();
  int err = _radio->startTransmit((uint8_t *) bytes, len);
  if (err == RADIOLIB_ERR_NONE) {
    return true;
  }
  n_spi_errors++;
  MESH_DEBUG_PRINTLN("RadioLibWrapper: error: startTransmit(%d)", err);
  idle();   // trigger another startRecv()
  _board->onAfterTransmit();
  return false;
}

bool RadioLibWrapper::isSendComplete() {
  bool completed = false;
  RADIO_STATE_ENTER();
  if (state & STATE_INT_READY) {
    state = STATE_IDLE;
    completed = true;
  }
  RADIO_STATE_EXIT();
  if (completed) {
    n_sent++;
    return true;
  }
  return false;
}

void RadioLibWrapper::onSendFinished() {
  _radio->finishTransmit();
  _board->onAfterTransmit();
  RADIO_STATE_ENTER();
  state = STATE_IDLE;
  RADIO_STATE_EXIT();
}

int16_t RadioLibWrapper::performChannelScan() {
  return _radio->scanChannel();
}

bool RadioLibWrapper::isChannelActive() {
  // int.thresh: RSSI-based interference detection (relative to noise floor)
  if (_threshold != 0 && getCurrentRSSI() > _noise_floor + _threshold) return true;

  // cad: hardware channel activity detection
  if (_cad_enabled) {
    int16_t result = performChannelScan();
    // scanChannel() triggers DIO interrupt (CAD done) which sets STATE_INT_READY
    // via setFlag() ISR. Clear it before restarting RX so recvRaw() doesn't
    // try to read a non-existent packet and count a spurious recv error.
    RADIO_STATE_ENTER();
    state = STATE_IDLE;
    RADIO_STATE_EXIT();
    startRecv();
    if (result == RADIOLIB_LORA_DETECTED || result == RADIOLIB_PREAMBLE_DETECTED) {
      return true;
    }
    if (result != RADIOLIB_CHANNEL_FREE && result != RADIOLIB_ERR_NONE) {
      n_spi_errors++;
      MESH_DEBUG_PRINTLN("RadioLibWrapper: error: scanChannel(%d)", (int)result);
    }
  }

  return false;
}

float RadioLibWrapper::getLastRSSI() const {
  return _radio->getRSSI();
}
float RadioLibWrapper::getLastSNR() const {
  return _radio->getSNR();
}

// Approximate SNR threshold per SF for successful reception (based on Semtech datasheets)
static float snr_threshold[] = {
    -7.5,  // SF7 needs at least -7.5 dB SNR
    -10,   // SF8 needs at least -10 dB SNR
    -12.5, // SF9 needs at least -12.5 dB SNR
    -15,  // SF10 needs at least -15 dB SNR
    -17.5,// SF11 needs at least -17.5 dB SNR
    -20   // SF12 needs at least -20 dB SNR
};

float RadioLibWrapper::packetScoreInt(float snr, int sf, int packet_len) {
  if (sf < 7) return 0.0f;

  if (snr < snr_threshold[sf - 7]) return 0.0f;    // Below threshold, no chance of success

  auto success_rate_based_on_snr = (snr - snr_threshold[sf - 7]) / 10.0;
  auto collision_penalty = 1 - (packet_len / 256.0);   // Assuming max packet of 256 bytes

  return max(0.0, min(1.0, success_rate_based_on_snr * collision_penalty));
}

PacketMillis RadioLibWrapper::calcMaxPacketMillis(uint8_t sf, float bw, uint8_t cr, uint8_t preambleSymbols) {
  // based on RadioLib's calculateTimeOnAir()
  uint32_t tsym_us = ((uint32_t)10000 << sf) / (bw * 10);
  uint32_t sfCoeff1_x4 = (sf == 5 || sf == 6) ? 25 : 17; // 6.25 : 4.25, semtech magic numbers to account for sync word + sfd

  // preamble + syncword + sfd + header
  uint32_t preamble_us = (((preambleSymbols + 8) * 4 + sfCoeff1_x4) * tsym_us) / 4;

  // airtime for max packet at current radio settings
  uint32_t total_us   = _radio->getTimeOnAir(MAX_TRANS_UNIT);
  // airtime for payload only (no preamble, header or SOF)
  uint32_t payload_us = total_us > preamble_us ? total_us - preamble_us : 4000 - preamble_us; // fallback to 4 secs at worst case
  // rescale payload_us for max possible CR
  if (cr >= 5 && cr < 8) { payload_us = (payload_us * 8) / cr; }

  return PacketMillis {(preamble_us + 999) / 1000, (payload_us + 999) / 1000};
}
