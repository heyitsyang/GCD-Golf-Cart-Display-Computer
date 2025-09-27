#include "espnow_task.h"
#include "config.h"
#include "globals.h"
#include "types.h"
#include "communication/espnow_handler.h"
#include "get_set_vars.h"

void espnowTask(void *parameter) {
    espnow_recv_item_t recv_item;
    uint32_t lastHeartbeat = 0;
    uint32_t lastGPSSend = 0;
    
    // Queue is created in main.cpp
    
    while (true) {
        // Check for peer timeouts
        if (espnow_enabled && espNow.isInitialized() && espnow_connected) {
            uint32_t now = millis();
            bool any_peer_online = false;
            bool has_communicated_peers = false;

            for (int i = 0; i < espNow.getPeerCount(); i++) {
                espnow_peer_info_t* peer = espNow.getPeerInfo(i);
                if (peer && peer->last_seen > 0) {
                    has_communicated_peers = true;
                    if (now - peer->last_seen < ESPNOW_PEER_TIMEOUT) {
                        any_peer_online = true;
                        break;
                    }
                }
            }

            // Disconnect if peers have timed out
            if (has_communicated_peers && !any_peer_online) {
                espnow_connected = false;
                set_var_espnow_connected(false);
                Serial.printf("*** ESP-NOW connection timeout ***\n");
            }
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

                    // Connection status is set when data is received from peers
                } else {
                    espnow_status = "Init failed";
                    Serial.println("ESP-NOW Task: Initialization failed");
                }
            } else {
                espNow.deinit();
                espnow_status = "Disabled";
                espnow_connected = false;
                set_var_espnow_connected(false);  // Update UI variable
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