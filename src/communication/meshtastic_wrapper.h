  #ifndef MESHTASTIC_WRAPPER_H
  #define MESHTASTIC_WRAPPER_H

  #include "Meshtastic.h"
  #include <HardwareSerial.h>

  class MeshtasticWrapper {
  private:
      uint32_t last_successful_operation;
      uint32_t consecutive_failures;
      uint32_t last_data_activity;
      bool is_initialized;

      // Recovery constants
      static const uint32_t RECOVERY_TIMEOUT = 300000;    // 5 minutes
      static const uint32_t MAX_FAILURES = 5;
      static const uint32_t POST_FLASH_DELAY = 3000;

      // Internal methods
      void performHardReset();
      bool isHealthy();
      void clearSerialBuffer();

  public:
      MeshtasticWrapper();

      // Wrapper methods that add robustness
      bool initialize(int8_t rx_pin, int8_t tx_pin, uint32_t baud);
      void shutdown();
      bool loop(uint32_t now);
      bool sendText(const char* text, uint32_t dest = BROADCAST_ADDR, uint8_t channel = 0);
      bool requestNodeReport(void (*callback)(mt_node_t*, mt_nr_progress_t));
      void setTextMessageCallback(void (*callback)(uint32_t, uint32_t, uint8_t, const char*));

      // Health monitoring
      uint32_t getLastActivity() const { return last_data_activity; }
      uint32_t getConsecutiveFailures() const { return consecutive_failures; }
      bool isConnected() const;
  };

  extern MeshtasticWrapper meshtasticWrapper;

  #endif