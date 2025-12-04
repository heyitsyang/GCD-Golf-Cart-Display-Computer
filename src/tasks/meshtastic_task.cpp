#include "meshtastic_task.h"
#include "config.h"
#include "globals.h"
#include "Meshtastic.h"
#include "communication/meshtastic_admin.h"
#include "get_set_vars.h"

// State tracking for wake notification
static bool wakeNotificationSent = false;

void sendWakeNotificationOnBoot() {
    // Only run once successfully
    if (wakeNotificationSent) {
        return;
    }

    // Wait for Meshtastic connection
    if (not_yet_connected) {
        return;
    }

    // Connection established - send wake notification
    const char *wakeMessage = "~#01#GC#AWAKE#";

    Serial.println("Sending wake notification broadcast...");

    if (mt_send_text(wakeMessage, BROADCAST_ADDR, 0)) {
        Serial.print("Wake notification sent: ");
        Serial.println(wakeMessage);
        wakeNotificationSent = true;
    } else {
        Serial.println("Failed to send wake notification, will retry");
    }
}

  void meshtasticTask(void *parameter) {
      static bool old_reboot_meshtastic = false;

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

          // Handle Meshtastic reboot trigger
          if (reboot_meshtastic && !old_reboot_meshtastic) {
              Serial.println("\n=== Meshtastic Reboot Triggered ===");
              if (mesh_serial_enabled) {
                  if (mt_send_admin_reboot(0)) {  // 0 = immediate reboot
                      Serial.println("Reboot command sent successfully");
                  } else {
                      Serial.println("Failed to send reboot command");
                  }
              } else {
                  Serial.println("Cannot reboot: Meshtastic serial is disabled");
              }
              // Reset the trigger
              reboot_meshtastic = false;
          }
          old_reboot_meshtastic = reboot_meshtastic;

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