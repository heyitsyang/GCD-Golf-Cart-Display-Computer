  #include "meshtastic_wrapper.h"
  #include "config.h"
  #include <soc/periph_defs.h>
  #include <driver/periph_ctrl.h>

  MeshtasticWrapper meshtasticWrapper;

  // Note: We cannot directly access meshSerial since it's internal to the library
  // We'll work around this by using the library's own functions

  MeshtasticWrapper::MeshtasticWrapper()
      : last_successful_operation(0)
      , consecutive_failures(0)
      , last_data_activity(0)
      , is_initialized(false) {
  }

  void MeshtasticWrapper::performHardReset() {
      Serial.println("MeshtasticWrapper: Performing recovery reset...");

      // Shutdown existing connection
      if (is_initialized) {
          mt_serial_end();
          delay(200);
      }

      // Hard reset UART2 peripheral (CYD-side only)
      periph_module_disable(PERIPH_UART2_MODULE);
      delay(100);
      periph_module_enable(PERIPH_UART2_MODULE);
      delay(100);

      // Clear any application-side state that might be stale
      last_successful_operation = millis();
      consecutive_failures = 0;
      last_data_activity = millis();

      Serial.println("MeshtasticWrapper: Reset complete");
  }

  void MeshtasticWrapper::clearSerialBuffer() {
      // Clear buffer by calling mt_loop repeatedly to process any stale data
      unsigned long start = millis();
      while ((millis() - start) < 1000) {
          // mt_loop will process any available data through the library
          mt_loop(millis());
          delay(10);
      }
  }

  bool MeshtasticWrapper::isHealthy() {
      uint32_t now = millis();

      // Check for extended period without any activity
      if ((now - last_data_activity) > RECOVERY_TIMEOUT) {
          Serial.println("MeshtasticWrapper: Health check failed - no activity timeout");
          return false;
      }

      // Check for too many consecutive failures
      if (consecutive_failures >= MAX_FAILURES) {
          Serial.println("MeshtasticWrapper: Health check failed - too many failures");
          return false;
      }

      return true;
  }

  bool MeshtasticWrapper::initialize(int8_t rx_pin, int8_t tx_pin, uint32_t baud) {
      Serial.println("MeshtasticWrapper: Initializing with post-flash recovery...");

      // Extended delay for post-firmware-flash stability
      delay(POST_FLASH_DELAY);

      // Perform hard reset to ensure clean state
      performHardReset();

      // Initialize the library
      mt_serial_init(rx_pin, tx_pin, baud);
      delay(1000);

      // Clear any residual data
      clearSerialBuffer();

      is_initialized = true;
      last_successful_operation = millis();
      last_data_activity = millis();

      Serial.println("MeshtasticWrapper: Initialization complete");
      return true;
  }

  void MeshtasticWrapper::shutdown() {
      if (is_initialized) {
          mt_serial_end();
          is_initialized = false;
      }
  }

  bool MeshtasticWrapper::loop(uint32_t now) {
      if (!is_initialized) return false;

      // Check health before processing
      if (!isHealthy()) {
          Serial.println("MeshtasticWrapper: Health check failed, attempting recovery...");
          performHardReset();
          mt_serial_init(MT_SERIAL_RX_PIN, MT_SERIAL_TX_PIN, MT_DEV_BAUD_RATE);
          delay(1000);
          clearSerialBuffer();
          return false;
      }

      // Store previous activity level (we'll compare after mt_loop)
      bool had_activity = false;

      // Call the library's loop function
      bool result = mt_loop(now);

      // We can't directly monitor the serial port, but we can infer activity
      // by checking if the library thinks it can send (indicates it's getting responses)
      if (result) {
          last_data_activity = now;
          had_activity = true;
      }

      // Update our tracking
      if (had_activity) {
          last_successful_operation = now;
          consecutive_failures = 0;
      }

      return result;
  }

  bool MeshtasticWrapper::sendText(const char* text, uint32_t dest, uint8_t channel) {
      if (!is_initialized) return false;

      // Attempt to send using the library
      bool success = mt_send_text(text, dest, channel);

      uint32_t now = millis();
      if (success) {
          last_successful_operation = now;
          last_data_activity = now;
          consecutive_failures = 0;
      } else {
          consecutive_failures++;
          Serial.printf("MeshtasticWrapper: Send failed (consecutive failures: %lu)\n", consecutive_failures);
      }

      return success;
  }

  bool MeshtasticWrapper::requestNodeReport(void (*callback)(mt_node_t*, mt_nr_progress_t)) {
      if (!is_initialized) return false;

      bool success = mt_request_node_report(callback);

      uint32_t now = millis();
      if (success) {
          last_successful_operation = now;
          consecutive_failures = 0;
      } else {
          consecutive_failures++;
      }

      return success;
  }

  void MeshtasticWrapper::setTextMessageCallback(void (*callback)(uint32_t, uint32_t, uint8_t, const char*)) {
      set_text_message_callback(callback);
  }

  bool MeshtasticWrapper::isConnected() const {
      uint32_t now = millis();
      return is_initialized &&
             (consecutive_failures < MAX_FAILURES) &&
             ((now - last_data_activity) < RECOVERY_TIMEOUT);
  }