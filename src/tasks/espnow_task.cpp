#include "espnow_task.h"
#include "config.h"
#include "globals.h"
#include "types.h"
#include "communication/espnow_handler.h"

void espnowTask(void *parameter) {
    espnow_recv_item_t recv_item;
    uint32_t lastHeartbeat = 0;
    uint32_t lastGPSSend = 0;
    
    // Queue is created in main.cpp
    
    while (true) {
        // Update ESP-NOW connection status
        bool current_espnow_connected = espnow_enabled && espNow.isInitialized() && (espNow.getPeerCount() > 0);
        if (current_espnow_connected != espnow_connected) {
            espnow_connected = current_espnow_connected;
            Serial.printf("*** ESPNOW_CONNECTED STATE CHANGE: %s ***\n",
                         espnow_connected ? "CONNECTED" : "DISCONNECTED");
            Serial.printf("    espnow_enabled: %s\n", espnow_enabled ? "true" : "false");
            Serial.printf("    initialized: %s\n", espNow.isInitialized() ? "true" : "false");
            Serial.printf("    peer count: %d\n", espNow.getPeerCount());
            Serial.printf("    status: %s\n", espNow.getStatus().c_str());
        }

        // Check if ESP-NOW should be enabled/disabled
        static bool first_run = true;
        if (espnow_enabled != old_espnow_enabled || first_run) {
            first_run = false;
            old_espnow_enabled = espnow_enabled;
            
            if (espnow_enabled) {
                // Initialize ESP-NOW
                if (espNow.init()) {
                    espnow_status = "Initializing...";
                    
                    // Add saved peer if exists
                    if (espnow_mac_addr != "NONE" && espnow_mac_addr.length() == 17) {
                        if (espNow.addPeerFromString(espnow_mac_addr, "Saved Peer")) {
                            Serial.println("ESP-NOW: Restored saved peer");
                        }
                    }
                    espnow_status = espNow.getStatus();

                    // Update connection status after peer setup
                    espnow_connected = espNow.getPeerCount() > 0;
                } else {
                    espnow_status = "Init failed";
                    Serial.println("ESP-NOW Task: Initialization failed");
                }
            } else {
                espNow.deinit();
                espnow_status = "Disabled";
                espnow_connected = false;
                Serial.println("ESP-NOW disabled");
            }
        }
        
        // Only process if enabled and initialized
        if (espnow_enabled && espNow.isInitialized()) {
            // Process received messages
            if (xQueueReceive(espnowRecvQueue, &recv_item, pdMS_TO_TICKS(10))) {
                espNow.processReceivedMessage(recv_item);
            }
            
            uint32_t now = millis();
            
            // Send periodic heartbeat
            if (now - lastHeartbeat > ESPNOW_HEARTBEAT_INTERVAL) {
                lastHeartbeat = now;
                uint8_t heartbeatData[4];
                memcpy(heartbeatData, &now, 4);
                espNow.broadcast(ESPNOW_MSG_HEARTBEAT, heartbeatData, 4);
                #if DEBUG_ESPNOW == 1
                Serial.println("ESP-NOW: Heartbeat sent");
                #endif
            }
            
            // Send GPS data periodically if available
            if (now - lastGPSSend > ESPNOW_GPS_SEND_INTERVAL) {
                if (xSemaphoreTake(gpsMutex, pdMS_TO_TICKS(100))) {
                    if (latitude.length() > 0 && longitude.length() > 0) {
                        lastGPSSend = now;

                        // Pack GPS data as JSON-like string
                        String gpsData = "{\"lat\":\"" + latitude +
                                       "\",\"lon\":\"" + longitude +
                                       "\",\"alt\":\"" + altitude +
                                       "\",\"spd\":" + String(avg_speed_calc) +  // Use float version for accuracy
                                       ",\"hdg\":\"" + heading +
                                       "\",\"sats\":\"" + sats_hdop + "\"}";

                        espNow.broadcast(ESPNOW_MSG_GPS_DATA,
                                       (uint8_t*)gpsData.c_str(), gpsData.length());

                        #if DEBUG_ESPNOW == 1
                        Serial.println("ESP-NOW: GPS data sent");
                        #endif
                    }
                    xSemaphoreGive(gpsMutex);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}