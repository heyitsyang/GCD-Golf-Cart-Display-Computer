#include "meshtastic_task.h"
#include "config.h"
#include "globals.h"
#include "Meshtastic.h"
#include "communication/meshtastic_admin.h"
#include "get_set_vars.h"

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
#if DEBUG_GCM_MESSAGES
                      Serial.println("GCM TX: Reboot command sent");
#endif
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

          // Send AWAKE once after handshake (tag 3) + GPS config
          if (can_send && handshakeComplete && gpsConfigAttempted) {
              if (!wakeNotificationSent) {
                  const char *wakeMessage = "~#01#GC#AWAKE#";
                  if (mt_send_text(wakeMessage, BROADCAST_ADDR, 0)) {
#if DEBUG_GCM_MESSAGES
                      Serial.print("GCM TX: ");
                      Serial.println(wakeMessage);
#endif
                      wakeNotificationSent = true;
                  } else {
                      Serial.println("Failed to send wake notification, will retry");
                  }
              }
          }

#if DEBUG_OTA_TX_TEST
          // Periodic OTA test: send T1, T2, T3... every 30s to verify OTA TX
          if (can_send && handshakeComplete && gpsConfigAttempted && now >= next_send_time) {
              static int otaTestCount = 0;
              otaTestCount++;
              uint32_t elapsed = now / 1000;
              char msg[48];
              snprintf(msg, sizeof(msg), "T%d %lus", otaTestCount, elapsed);

              bool success = mt_send_text(msg, BROADCAST_ADDR, DEBUG_OTA_TX_TEST_CHANNEL);
              Serial.printf("OTA test: %s -> %s\n", msg, success ? "sent" : "FAILED");

              next_send_time = now + 30000;
          }
#endif

          vTaskDelay(pdMS_TO_TICKS(100));
      }
  }