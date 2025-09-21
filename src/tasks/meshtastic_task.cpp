#include "meshtastic_task.h"
#include "config.h"
#include "globals.h"
#include "communication/meshtastic_wrapper.h"  // Use wrapper instead of direct library

  void meshtasticTask(void *parameter) {
      while (true) {
          // Handle mesh communication enable/disable
          if (mesh_comm != old_mesh_comm) {
              old_mesh_comm = mesh_comm;
              if (mesh_comm == false) {
                  meshtasticWrapper.shutdown();
                  Serial.println("meshSerial disabled via wrapper");
              } else {
                  meshtasticWrapper.initialize(MT_SERIAL_RX_PIN, MT_SERIAL_TX_PIN, MT_DEV_BAUD_RATE);
                  Serial.println("meshSerial enabled via wrapper");
              }
          }

          uint32_t now = millis();
          bool can_send = meshtasticWrapper.loop(now);  // Use wrapper

          // Send periodic test message
          if (can_send && now >= next_send_time) {
              Serial.print("\nSending test message at: ");
              Serial.println(hhmm_str);

              uint32_t dest = BROADCAST_ADDR;
              uint8_t channel_index = 0;

              bool success = meshtasticWrapper.sendText("Hello, world from the CYD Golf Cart Computer!",
                                                       dest, channel_index);  // Use wrapper
              Serial.print("meshtasticWrapper.sendText returned: ");
              Serial.println(success ? "SUCCESS" : "***** FAILED ****");

              // Show health status
              if (!success) {
                  Serial.printf("Wrapper health - consecutive failures: %lu, last activity: %lu ms ago\n",
                               meshtasticWrapper.getConsecutiveFailures(),
                               now - meshtasticWrapper.getLastActivity());
              }

              next_send_time = now + SEND_PERIOD * 1000;
              Serial.printf("Next send time in: %d minutes\n", (SEND_PERIOD / 60));
          }

          vTaskDelay(pdMS_TO_TICKS(100));
      }
  }