#include "meshtastic_task.h"
#include "config.h"
#include "globals.h"
#include "Meshtastic.h"

  void meshtasticTask(void *parameter) {
      while (true) {
          uint32_t now = millis();

          // Handle mesh serial enable/disable
          if (mesh_serial_enabled != old_mesh_serial_enabled) {
              old_mesh_serial_enabled = mesh_serial_enabled;
              if (mesh_serial_enabled == false) {
                  mt_serial_end();
                  Serial.println("Meshtastic serial disabled");
              } else {
                  mt_serial_init(MT_SERIAL_RX_PIN, MT_SERIAL_TX_PIN, MT_DEV_BAUD_RATE);
                  Serial.println("Meshtastic serial enabled");
              }
          }

          // Call mt_loop if mesh_serial_enabled is enabled
          bool can_send = false;
          if (mesh_serial_enabled) {
              can_send = mt_loop(now);
          }

          // Send periodic test message
          if (can_send && now >= next_send_time) {
              Serial.print("\nSending test message at: ");
              Serial.println(hhmm_str);

              uint32_t dest = BROADCAST_ADDR;
              uint8_t channel_index = 0;

              bool success = mt_send_text("Hello, world from the CYD Golf Cart Computer!",
                                         dest, channel_index);
              Serial.print("mt_send_text returned: ");
              Serial.println(success ? "SUCCESS" : "***** FAILED ****");

              next_send_time = now + SEND_PERIOD * 1000;
              Serial.printf("Next send time in: %d minutes\n", (SEND_PERIOD / 60));
          }

          vTaskDelay(pdMS_TO_TICKS(100));
      }
  }