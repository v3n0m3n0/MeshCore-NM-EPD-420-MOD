#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <stdint.h>
#include <stddef.h>

class NMEPD420SDLogger {
public:
  enum class RfDir : uint8_t {
    RX_RAW  = 0,
    RX_PKT  = 1,
    TX_PKT  = 2,
    TX_FAIL = 3
  };

  struct RfLogRecord {
    uint32_t seq;
    uint32_t uptime_ms;
    uint32_t unix_ts;
    int16_t  rssi_dbm;
    int16_t  snr_q4;
    uint8_t  dir;
    uint8_t  flags;
    uint16_t payload_len;
    uint8_t  payload[256];
  };

  static constexpr size_t RING_CAPACITY = 32;
  static constexpr uint16_t PCAP_SNAPLEN = 264;
  static constexpr uint32_t PCAP_DLT_USER0 = 147;

  bool begin(SPIClass* hspi, uint8_t cs_pin, uint8_t lora_nss_pin);
  bool retryMount();
  void end();

  void setEnabled(bool enable) { _enabled = enable; }
  bool isEnabled() const { return _enabled; }
  bool isMounted() const { return _mounted; }
  uint32_t getCardSizeMB() const { return _card_size_mb; }

  void pushFrame(RfDir dir, const uint8_t* data, size_t len, float rssi_dbm, float snr_db, uint32_t unix_ts);
  void loop(bool radio_busy);

  size_t getRingCount() const { return _count; }
  size_t getRingCapacity() const { return RING_CAPACITY; }
  uint32_t getTotalQueued() const { return _total_queued; }
  uint32_t getTotalWritten() const { return _total_written; }
  uint32_t getDroppedCount() const { return _dropped_overflow; }
  uint32_t getWriteErrors() const { return _write_errors; }

  void clearBufferAndStats();
  bool handleCliCommand(const char* cmd, char* reply, size_t reply_sz = 150);
  void formatStatus(char* out, size_t out_sz) const;
  void formatUiSummary(char* out, size_t out_sz) const;
  void buildPcapTestSummary(char* out, size_t out_sz) const;

  static size_t buildPcapGlobalHeader(uint8_t out24[24]);
  static size_t buildPcapPacketRecord(const RfLogRecord& rec, uint8_t* out_buf, size_t out_cap);

private:
  bool mountCard();
  bool flushBatch(size_t max_records);

  SPIClass* _spi = nullptr;
  uint8_t _cs_pin = 255;
  uint8_t _lora_nss_pin = 255;

  bool _initialized = false;
  bool _mounted = false;
  bool _enabled = true;
  uint32_t _card_size_mb = 0;

  RfLogRecord _ring[RING_CAPACITY] = {};
  size_t _head = 0;
  size_t _tail = 0;
  size_t _count = 0;

  uint32_t _next_seq = 1;
  uint32_t _total_queued = 0;
  uint32_t _total_written = 0;
  uint32_t _dropped_overflow = 0;
  uint32_t _write_errors = 0;
  unsigned long _last_flush_ms = 0;
};
