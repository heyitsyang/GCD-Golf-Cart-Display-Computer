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
    static bool lastPairState = false;
    uint32_t pairingStartTime = 0;
    const uint32_t PAIRING_TIMEOUT_MS = 6000;  // Keep pairing window open for 6 seconds

    // Variables to save pre-pairing state
    String saved_mac_addr = "";
    bool pairing_succeeded = false;

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
                    if (espnow_gci_mac_addr != "NONE" && espnow_gci_mac_addr.length() == 17) {
                        if (espNow.addPeerFromString(espnow_gci_mac_addr, "Saved Peer")) {
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
            // Check for GCI pairing request
            if (espnow_pair_gci && !lastPairState) {
                lastPairState = true;
                pairingStartTime = millis();
                pairing_succeeded = false;

                // Save current MAC address before clearing peers
                saved_mac_addr = espnow_gci_mac_addr;
                Serial.printf("ESP-NOW: Starting pairing (saved MAC: %s)\n", saved_mac_addr.c_str());

                // Clear any existing peers to start fresh pairing
                int peerCount = espNow.getPeerCount();
                for (int i = peerCount - 1; i >= 0; i--) {
                    espnow_peer_info_t* peer = espNow.getPeerInfo(i);
                    if (peer) {
                        espNow.removePeer(peer->mac_addr);
                    }
                }
                Serial.println("ESP-NOW: Cleared all peers for fresh pairing");

                // Prepare RAW pairing command (no wrapper for initial pairing)
                structMsgToGci cmdData;
                cmdData.cmdNumber = GCI_CMD_ADD_PEER;

                // Get our MAC address directly (avoid String allocation)
                uint8_t myMacBytes[6];
                esp_read_mac(myMacBytes, ESP_MAC_WIFI_STA);
                memcpy(cmdData.macAddr, myMacBytes, 6);

                // Temporarily add broadcast peer for RAW pairing
                uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                espNow.addPeer(broadcastAddr, "Broadcast-Temp");

                // Broadcast RAW command to reach virgin GCI devices
                if (esp_now_send(broadcastAddr, (uint8_t*)&cmdData, sizeof(cmdData)) == ESP_OK) {
                    Serial.printf("ESP-NOW: RAW pairing command broadcast with MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
                                 myMacBytes[0], myMacBytes[1], myMacBytes[2],
                                 myMacBytes[3], myMacBytes[4], myMacBytes[5]);
                    Serial.println("ESP-NOW: Waiting for GCI to pair and switch to wrapped mode...");
                } else {
                    Serial.println("ESP-NOW: Failed to broadcast RAW pairing command");
                }

                // Remove broadcast peer immediately after sending
                espNow.removePeer(broadcastAddr);

                // Note: espnow_pair_gci stays true to keep pairing window open for ACK
            } else if (!espnow_pair_gci && lastPairState) {
                lastPairState = false;
            }

            // Check for pairing timeout - close the pairing window after timeout
            if (espnow_pair_gci && pairingStartTime > 0 &&
                (millis() - pairingStartTime) > PAIRING_TIMEOUT_MS) {
                Serial.println("ESP-NOW: Pairing timeout");

                // Check if pairing succeeded (MAC changed from saved value)
                if (espnow_gci_mac_addr != saved_mac_addr) {
                    Serial.printf("ESP-NOW: Pairing succeeded - new MAC: %s\n", espnow_gci_mac_addr.c_str());
                    pairing_succeeded = true;
                } else {
                    Serial.println("ESP-NOW: Pairing failed - restoring previous state");

                    // Restore saved peer if it was valid
                    if (saved_mac_addr != "NONE" && saved_mac_addr.length() == 17) {
                        if (espNow.addPeerFromString(saved_mac_addr, "Restored Peer")) {
                            Serial.printf("ESP-NOW: Restored previous peer: %s\n", saved_mac_addr.c_str());
                        } else {
                            Serial.println("ESP-NOW: Failed to restore previous peer");
                        }
                    }
                }

                espnow_pair_gci = false;
                set_var_espnow_pair_gci(false);
                pairingStartTime = 0;
            }

            // Process received messages
            if (xQueueReceive(espnowRecvQueue, &recv_item, pdMS_TO_TICKS(10))) {
                espNow.processReceivedMessage(recv_item);
            }

            uint32_t now = millis();

            // Send periodic heartbeat (only if we have peers)
            if (espNow.getPeerCount() > 0 && now - lastHeartbeat > ESPNOW_HEARTBEAT_INTERVAL) {
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