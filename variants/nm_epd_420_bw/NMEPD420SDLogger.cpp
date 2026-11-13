#include "NMEPD420SDLogger.h"

#include <SD.h>
#include <stdio.h>
#include <string.h>

namespace {

constexpr const char* PCAP_LOG_PATH = "/mesh_capture.pcap";
constexpr const char* TEXT_LOG_PATH = "/mesh_rf.log";
constexpr uint32_t SD_SPI_FREQ_HZ = 4000000UL;
constexpr unsigned long SD_FLUSH_INTERVAL_MS = 250UL;
constexpr size_t SD_FLUSH_THRESHOLD = 8;
constexpr size_t SD_MAX_BATCH_PER_LOOP = 2;

inline void writeLe16(uint8_t* dst, uint16_t v) {
  dst[0] = static_cast<uint8_t>(v & 0xFF);
  dst[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

inline void writeLe32(uint8_t* dst, uint32_t v) {
  dst[0] = static_cast<uint8_t>(v & 0xFF);
  dst[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
  dst[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
  dst[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

const char* dirLabel(uint8_t dir) {
  switch (static_cast<NMEPD420SDLogger::RfDir>(dir)) {
    case NMEPD420SDLogger::RfDir::RX_RAW:  return "RX_RAW";
    case NMEPD420SDLogger::RfDir::RX_PKT:  return "RX";
    case NMEPD420SDLogger::RfDir::TX_PKT:  return "TX";
    case NMEPD420SDLogger::RfDir::TX_FAIL: return "TX_FAIL";
    default: return "UNK";
  }
}

int8_t clampInt8(int32_t v) {
  if (v < -128) return -128;
  if (v > 127) return 127;
  return static_cast<int8_t>(v);
}

}  // namespace

size_t NMEPD420SDLogger::buildPcapGlobalHeader(uint8_t out24[24]) {
  writeLe32(&out24[0], 0xA1B2C3D4UL);
  writeLe16(&out24[4], 2);
  writeLe16(&out24[6], 4);
  writeLe32(&out24[8], 0);
  writeLe32(&out24[12], 0);
  writeLe32(&out24[16], PCAP_SNAPLEN);
  writeLe32(&out24[20], PCAP_DLT_USER0);
  return 24;
}

size_t NMEPD420SDLogger::buildPcapPacketRecord(const RfLogRecord& rec, uint8_t* out_buf, size_t out_cap) {
  size_t payload_len = rec.payload_len;
  if (payload_len > sizeof(rec.payload)) {
    payload_len = sizeof(rec.payload);
  }
  const size_t total_len = 16 + 4 + payload_len;
  if (!out_buf || out_cap < total_len) {
    return 0;
  }

  const uint32_t ts_sec = rec.unix_ts ? rec.unix_ts : (rec.uptime_ms / 1000UL);
  const uint32_t ts_usec = (rec.uptime_ms % 1000UL) * 1000UL;
  const uint32_t incl_len = static_cast<uint32_t>(4 + payload_len);

  writeLe32(&out_buf[0], ts_sec);
  writeLe32(&out_buf[4], ts_usec);
  writeLe32(&out_buf[8], incl_len);
  writeLe32(&out_buf[12], incl_len);

  // 4-byte MeshCore RF tap header for Wireshark DLT_USER0 dissector
  out_buf[16] = rec.dir;
  out_buf[17] = rec.flags;
  out_buf[18] = static_cast<uint8_t>(clampInt8(rec.rssi_dbm));
  out_buf[19] = static_cast<uint8_t>(clampInt8(rec.snr_q4));

  if (payload_len > 0) {
    memcpy(&out_buf[20], rec.payload, payload_len);
  }
  return total_len;
}

bool NMEPD420SDLogger::begin(SPIClass* hspi, uint8_t cs_pin, uint8_t lora_nss_pin) {
  _spi = hspi;
  _cs_pin = cs_pin;
  _lora_nss_pin = lora_nss_pin;
  _initialized = true;

  pinMode(_cs_pin, OUTPUT);
  digitalWrite(_cs_pin, HIGH);
  if (_lora_nss_pin != 255) {
    pinMode(_lora_nss_pin, OUTPUT);
    digitalWrite(_lora_nss_pin, HIGH);
  }

  return mountCard();
}

bool NMEPD420SDLogger::mountCard() {
  if (!_initialized || !_spi || _cs_pin == 255) {
    _mounted = false;
    _card_size_mb = 0;
    return false;
  }

  // Never contend on HSPI if SX1262 chip select is held low.
  if (_lora_nss_pin != 255 && digitalRead(_lora_nss_pin) == LOW) {
    _mounted = false;
    _card_size_mb = 0;
    return false;
  }

  digitalWrite(_cs_pin, HIGH);
  if (!SD.begin(_cs_pin, *_spi, SD_SPI_FREQ_HZ)) {
    SD.end();
    pinMode(_cs_pin, OUTPUT);
    digitalWrite(_cs_pin, HIGH);
    _mounted = false;
    _card_size_mb = 0;
    return false;
  }

  uint8_t card_type = SD.cardType();
  if (card_type == CARD_NONE) {
    SD.end();
    pinMode(_cs_pin, OUTPUT);
    digitalWrite(_cs_pin, HIGH);
    _mounted = false;
    _card_size_mb = 0;
    return false;
  }

  _card_size_mb = static_cast<uint32_t>(SD.cardSize() / (1024ULL * 1024ULL));
  _mounted = true;
  _last_flush_ms = millis();
  return true;
}

bool NMEPD420SDLogger::retryMount() {
  if (_mounted) {
    SD.end();
    _mounted = false;
  }
  return mountCard();
}

void NMEPD420SDLogger::end() {
  if (_mounted) {
    SD.end();
    _mounted = false;
  }
  _card_size_mb = 0;
  if (_cs_pin != 255) {
    pinMode(_cs_pin, OUTPUT);
    digitalWrite(_cs_pin, HIGH);
  }
}

void NMEPD420SDLogger::pushFrame(RfDir dir, const uint8_t* data, size_t len, float rssi_dbm, float snr_db, uint32_t unix_ts) {
  if (!_enabled || !data || len == 0) {
    return;
  }

  const size_t copy_len = len > sizeof(RfLogRecord::payload) ? sizeof(RfLogRecord::payload) : len;
  if (_count >= RING_CAPACITY) {
    _tail = (_tail + 1) % RING_CAPACITY;
    _count = RING_CAPACITY - 1;
    _dropped_overflow++;
  }

  RfLogRecord& dst = _ring[_head];
  dst.seq = _next_seq++;
  dst.uptime_ms = millis();
  dst.unix_ts = unix_ts;
  dst.rssi_dbm = static_cast<int16_t>(rssi_dbm);
  dst.snr_q4 = static_cast<int16_t>(snr_db * 4.0f);
  dst.dir = static_cast<uint8_t>(dir);
  dst.flags = (copy_len < len) ? 0x01 : 0x00;
  dst.payload_len = static_cast<uint16_t>(copy_len);
  memcpy(dst.payload, data, copy_len);

  _head = (_head + 1) % RING_CAPACITY;
  _count++;
  _total_queued++;
}

bool NMEPD420SDLogger::flushBatch(size_t max_records) {
  fs::File pcap_file = SD.open(PCAP_LOG_PATH, FILE_APPEND);
  if (!pcap_file) {
    return false;
  }

  if (pcap_file.size() == 0) {
    uint8_t hdr[24];
    buildPcapGlobalHeader(hdr);
    if (pcap_file.write(hdr, sizeof(hdr)) != sizeof(hdr)) {
      pcap_file.close();
      return false;
    }
  }

  fs::File text_file = SD.open(TEXT_LOG_PATH, FILE_APPEND);

  size_t written_now = 0;
  uint8_t rec_buf[16 + 4 + 256];
  while (_count > 0 && written_now < max_records) {
    const RfLogRecord& rec = _ring[_tail];
    size_t rec_len = buildPcapPacketRecord(rec, rec_buf, sizeof(rec_buf));
    if (rec_len == 0 || pcap_file.write(rec_buf, rec_len) != rec_len) {
      pcap_file.close();
      if (text_file) text_file.close();
      return false;
    }

    if (text_file) {
      text_file.printf("%lu,%lu,%s,%u,%d,%.2f\n",
                       static_cast<unsigned long>(rec.seq),
                       static_cast<unsigned long>(rec.uptime_ms),
                       dirLabel(rec.dir),
                       static_cast<unsigned>(rec.payload_len),
                       static_cast<int>(rec.rssi_dbm),
                       static_cast<double>(rec.snr_q4) / 4.0);
    }

    _tail = (_tail + 1) % RING_CAPACITY;
    _count--;
    _total_written++;
    written_now++;
  }

  pcap_file.close();
  if (text_file) {
    text_file.close();
  }
  return true;
}

void NMEPD420SDLogger::loop(bool radio_busy) {
  if (!_mounted || !_enabled || _count == 0) {
    return;
  }

  // Shared HSPI arbitration: yield whenever SX1262 is transmitting/CAD or NSS is active.
  if (radio_busy || (_lora_nss_pin != 255 && digitalRead(_lora_nss_pin) == LOW)) {
    return;
  }

  const unsigned long now = millis();
  if (_count < SD_FLUSH_THRESHOLD && static_cast<unsigned long>(now - _last_flush_ms) < SD_FLUSH_INTERVAL_MS) {
    return;
  }

  _last_flush_ms = now;
  if (!flushBatch(SD_MAX_BATCH_PER_LOOP)) {
    _write_errors++;
    end();
  }
}

void NMEPD420SDLogger::clearBufferAndStats() {
  _head = 0;
  _tail = 0;
  _count = 0;
  _total_queued = 0;
  _total_written = 0;
  _dropped_overflow = 0;
  _write_errors = 0;
}

bool NMEPD420SDLogger::handleCliCommand(const char* cmd, char* reply, size_t reply_sz) {
  if (!cmd || !reply || reply_sz == 0) return false;
  if (strcmp(cmd, "sd status") == 0) {
    formatStatus(reply, reply_sz);
    return true;
  }
  if (strcmp(cmd, "sd retry") == 0) {
    retryMount();
    formatStatus(reply, reply_sz);
    return true;
  }
  if (strcmp(cmd, "sd on") == 0) {
    setEnabled(true);
    formatStatus(reply, reply_sz);
    return true;
  }
  if (strcmp(cmd, "sd off") == 0) {
    setEnabled(false);
    formatStatus(reply, reply_sz);
    return true;
  }
  if (strcmp(cmd, "sd clear") == 0) {
    clearBufferAndStats();
    formatStatus(reply, reply_sz);
    return true;
  }
  if (strcmp(cmd, "sd pcap-test") == 0) {
    buildPcapTestSummary(reply, reply_sz);
    return true;
  }
  if (strcmp(cmd, "sd ring-test") == 0) {
    const uint8_t sample_rx[8] = {0x11, 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    const uint8_t sample_tx[6] = {0x22, 0x01, 0x10, 0x20, 0x30, 0x40};
    pushFrame(RfDir::RX_RAW, sample_rx, sizeof(sample_rx), -89.0f, 8.5f, 0);
    pushFrame(RfDir::TX_PKT, sample_tx, sizeof(sample_tx), 0.0f, 0.0f, 0);
    formatStatus(reply, reply_sz);
    return true;
  }
  return false;
}

void NMEPD420SDLogger::formatStatus(char* out, size_t out_sz) const {
  if (!out || out_sz == 0) return;
  if (_mounted) {
    snprintf(out, out_sz,
             "sd:mounted en:%s mb:%lu ring:%u/%u q:%lu wr:%lu drop:%lu err:%lu",
             _enabled ? "on" : "off",
             static_cast<unsigned long>(_card_size_mb),
             static_cast<unsigned>(_count),
             static_cast<unsigned>(RING_CAPACITY),
             static_cast<unsigned long>(_total_queued),
             static_cast<unsigned long>(_total_written),
             static_cast<unsigned long>(_dropped_overflow),
             static_cast<unsigned long>(_write_errors));
  } else {
    snprintf(out, out_sz,
             "sd:absent en:%s ring:%u/%u q:%lu wr:%lu drop:%lu err:%lu",
             _enabled ? "on" : "off",
             static_cast<unsigned>(_count),
             static_cast<unsigned>(RING_CAPACITY),
             static_cast<unsigned long>(_total_queued),
             static_cast<unsigned long>(_total_written),
             static_cast<unsigned long>(_dropped_overflow),
             static_cast<unsigned long>(_write_errors));
  }
}

void NMEPD420SDLogger::formatUiSummary(char* out, size_t out_sz) const {
  if (!out || out_sz == 0) return;
  if (_mounted) {
    snprintf(out, out_sz, "OK %luwr (%u/%u)",
             static_cast<unsigned long>(_total_written),
             static_cast<unsigned>(_count),
             static_cast<unsigned>(RING_CAPACITY));
  } else {
    snprintf(out, out_sz, "Absent (%u/%u)",
             static_cast<unsigned>(_count),
             static_cast<unsigned>(RING_CAPACITY));
  }
}

void NMEPD420SDLogger::buildPcapTestSummary(char* out, size_t out_sz) const {
  if (!out || out_sz == 0) return;
  uint8_t frame[24 + 16 + 4 + 4] = {};
  size_t g_len = buildPcapGlobalHeader(frame);

  RfLogRecord sample = {};
  sample.seq = _next_seq;
  sample.uptime_ms = millis();
  sample.unix_ts = 1700000000UL;
  sample.rssi_dbm = -88;
  sample.snr_q4 = 38;
  sample.dir = static_cast<uint8_t>(RfDir::RX_RAW);
  sample.flags = 0;
  sample.payload_len = 4;
  sample.payload[0] = 0x11;
  sample.payload[1] = 0x22;
  sample.payload[2] = 0x33;
  sample.payload[3] = 0x44;

  size_t r_len = buildPcapPacketRecord(sample, &frame[g_len], sizeof(frame) - g_len);
  snprintf(out, out_sz,
           "pcap:%uB hdr:%02X%02X%02X%02X dlt:%lu pkt:%uB",
           static_cast<unsigned>(g_len + r_len),
           frame[0], frame[1], frame[2], frame[3],
           static_cast<unsigned long>(PCAP_DLT_USER0),
           static_cast<unsigned>(r_len));
}
