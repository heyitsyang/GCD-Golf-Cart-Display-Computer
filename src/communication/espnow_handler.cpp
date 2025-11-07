#include "espnow_handler.h"
#include "config.h"
#include "globals.h"
#include "get_set_vars.h"
#include <esp_wifi.h>

// Global instance
ESPNowHandler espNow;

bool ESPNowHandler::init() {
    if (initialized) {
        return true;
    }
    
    // Initialize WiFi in station mode
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    // Set WiFi channel
    int32_t channel = ESPNOW_CHANNEL;
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    
    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        status = "Init failed";
        return false;
    }
    
    // Register callbacks
    esp_now_register_send_cb(espnowOnDataSent);
    esp_now_register_recv_cb(espnowOnDataRecv);
    
    // Set ESP-NOW rate for long range
    esp_wifi_config_espnow_rate(WIFI_IF_STA, WIFI_PHY_RATE_LORA_250K);
    
    initialized = true;
    status = "Ready";
    espnow_connected = false;  // No peers yet
    set_var_espnow_connected(false);  // Update UI variable
    Serial.println("ESP-NOW initialized successfully");
    Serial.print("MAC Address: ");
    Serial.println(getMyMacAddress());

    return true;
}

void ESPNowHandler::deinit() {
    if (!initialized) {
        return;
    }

    esp_now_deinit();
    initialized = false;
    peer_count = 0;
    status = "Disabled";
    espnow_connected = false;
    set_var_espnow_connected(false);  // Update UI variable
    Serial.println("ESP-NOW deinitialized");
}

bool ESPNowHandler::restart() {
    Serial.println("ESP-NOW: Restarting...");
    deinit();
    return init();
}

bool ESPNowHandler::addPeer(const uint8_t *mac_addr, const char* name) {
    if (peer_count >= ESPNOW_MAX_PEER_NUM) {
        Serial.println("ESP-NOW: Max peers reached");
        return false;
    }
    
    // Check if already exists
    if (isPeerRegistered(mac_addr)) {
        Serial.println("ESP-NOW: Peer already registered");
        return true;
    }
    
    // Remove peer first if it exists (ESP-NOW might have auto-added it)
    esp_now_del_peer(mac_addr);

    // Add to ESP-NOW with correct settings
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac_addr, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("ESP-NOW: Failed to add peer");
        return false;
    }
    
    // Store in our list
    memcpy(peers[peer_count].mac_addr, mac_addr, 6);
    if (name) {
        strncpy(peers[peer_count].name, name, 31);
    } else {
        strcpy(peers[peer_count].name, "Unknown");
    }
    peers[peer_count].is_online = false;
    peers[peer_count].last_seen = 0;
    peers[peer_count].last_rssi = 0;
    peer_count++;
    
    Serial.printf("ESP-NOW: Peer added - %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac_addr[0], mac_addr[1], mac_addr[2],
                  mac_addr[3], mac_addr[4], mac_addr[5]);

    espnow_peer_count = peer_count;
    status = String("Connected (") + String(peer_count) + " peers)";

    return true;
}

bool ESPNowHandler::addPeerFromString(const String& mac_str, const char* name) {
    uint8_t mac_bytes[6];
    if (macStringToBytes(mac_str, mac_bytes)) {
        return addPeer(mac_bytes, name);
    }
    return false;
}

bool ESPNowHandler::removePeer(const uint8_t *mac_addr) {
    if (esp_now_del_peer(mac_addr) != ESP_OK) {
        return false;
    }
    
    // Remove from our list
    for (int i = 0; i < peer_count; i++) {
        if (memcmp(peers[i].mac_addr, mac_addr, 6) == 0) {
            // Shift remaining peers
            for (int j = i; j < peer_count - 1; j++) {
                peers[j] = peers[j + 1];
            }
            peer_count--;
            espnow_peer_count = peer_count;
            status = String("Connected (") + String(peer_count) + " peers)";

            // If no peers left, definitely disconnect
            if (peer_count == 0) {
                bool was_connected = espnow_connected;
                espnow_connected = false;
                set_var_espnow_connected(false);  // Update UI variable

                if (was_connected) {
                    Serial.printf("*** ESP-NOW peer removed - connection state changed to: %s ***\n", "DISCONNECTED");
                }
            }

            break;
        }
    }
    
    return true;
}

bool ESPNowHandler::isPeerRegistered(const uint8_t *mac_addr) {
    return esp_now_is_peer_exist(mac_addr);
}

espnow_peer_info_t* ESPNowHandler::getPeerInfo(int index) {
    if (index >= 0 && index < peer_count) {
        return &peers[index];
    }
    return nullptr;
}

bool ESPNowHandler::sendMessage(const uint8_t *mac_addr, espnow_msg_type_t type,
                                const uint8_t *data, size_t len) {
    if (!initialized || len > ESPNOW_MAX_PAYLOAD) {
        return false;
    }

    espnow_message_t msg;
    msg.type = type;
    msg.timestamp = millis();
    msg.msg_id = getNextMessageId();
    msg.data_len = len;
    if (len > 0) {
        memcpy(msg.data, data, len);
    }

    // Send only header + actual payload (not full 249-byte structure)
    return sendRawData(mac_addr, (uint8_t*)&msg, ESPNOW_PACKET_SIZE(len));
}

bool ESPNowHandler::sendTextMessage(const uint8_t *mac_addr, const String& text) {
    return sendMessage(mac_addr, ESPNOW_MSG_TEXT, 
                      (uint8_t*)text.c_str(), text.length());
}

bool ESPNowHandler::broadcast(espnow_msg_type_t type, const uint8_t *data, size_t len) {
    if (peer_count == 0) {
        return false; // No peers to send to
    }

    bool success = true;
    for (int i = 0; i < peer_count; i++) {
        // Verify peer is still registered in ESP-NOW before sending
        if (esp_now_is_peer_exist(peers[i].mac_addr)) {
            // Use sendMessage to wrap data in espnow_message_t
            if (!sendMessage(peers[i].mac_addr, type, data, len)) {
                success = false;
            }
        } else {
            Serial.printf("ESP-NOW: Peer not in ESP-NOW list: %02X:%02X:%02X:%02X:%02X:%02X\n",
                         peers[i].mac_addr[0], peers[i].mac_addr[1], peers[i].mac_addr[2],
                         peers[i].mac_addr[3], peers[i].mac_addr[4], peers[i].mac_addr[5]);
            success = false;
        }
    }
    return success;
}

bool ESPNowHandler::broadcastText(const String& text) {
    return broadcast(ESPNOW_MSG_TEXT, (uint8_t*)text.c_str(), text.length());
}

bool ESPNowHandler::sendGolfCartCommand(const uint8_t *mac_addr, gci_command_t cmdNumber, const void *payload, size_t payloadSize) {
    if (!initialized) {
        return false;
    }

    // Prepare command data in golf cart format
    dataToGci.cmdNumber = cmdNumber;
    cmdToGci = cmdNumber;  // Update global variable

    // Copy payload if provided (up to available space in macAddr field)
    if (payload != nullptr && payloadSize > 0) {
        size_t copySize = min(payloadSize, sizeof(dataToGci.macAddr));
        memcpy(dataToGci.macAddr, payload, copySize);

        // Clear remaining bytes if payload is smaller than field
        if (copySize < sizeof(dataToGci.macAddr)) {
            memset(&dataToGci.macAddr[copySize], 0, sizeof(dataToGci.macAddr) - copySize);
        }
    } else {
        memset(dataToGci.macAddr, 0, sizeof(dataToGci.macAddr));  // Clear if no payload
    }

    // Send raw struct data (no wrapper)
    return sendRawData(mac_addr, (uint8_t*)&dataToGci, sizeof(structMsgToGci));
}

bool ESPNowHandler::sendRawData(const uint8_t *mac_addr, const uint8_t *data, size_t len) {
    for (int retry = 0; retry < ESPNOW_SEND_RETRY_COUNT; retry++) {
        esp_err_t result = esp_now_send(mac_addr, data, len);
        if (result == ESP_OK) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(ESPNOW_SEND_RETRY_DELAY));
    }
    return false;
}

String ESPNowHandler::getMyMacAddress() {
    return WiFi.macAddress();
}

bool ESPNowHandler::macStringToBytes(const String& mac_str, uint8_t* mac_bytes) {
    if (mac_str.length() != 17) return false;
    
    int values[6];
    if (sscanf(mac_str.c_str(), "%x:%x:%x:%x:%x:%x",
               &values[0], &values[1], &values[2],
               &values[3], &values[4], &values[5]) == 6) {
        for (int i = 0; i < 6; i++) {
            mac_bytes[i] = (uint8_t)values[i];
        }
        return true;
    }
    return false;
}

void ESPNowHandler::processReceivedMessage(espnow_recv_item_t &item) {
    // Update peer info and connection status
    for (int i = 0; i < peer_count; i++) {
        if (memcmp(peers[i].mac_addr, item.mac_addr, 6) == 0) {
            peers[i].is_online = true;
            peers[i].last_seen = millis();
            peers[i].last_rssi = item.rssi;

            // Set connected status when we receive data from a known peer
            if (!espnow_connected) {
                espnow_connected = true;
                set_var_espnow_connected(true);
                Serial.printf("*** ESP-NOW connection established ***\n");
            }
            break;
        }
    }
    
    // Process based on message type
    char mac_str[18];
    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",
            item.mac_addr[0], item.mac_addr[1], item.mac_addr[2],
            item.mac_addr[3], item.mac_addr[4], item.mac_addr[5]);
    
    switch (item.message.type) {
        case ESPNOW_MSG_TEXT: {
            String text((char*)item.message.data);
            espnow_last_received = String(mac_str) + ": " + text;
            Serial.print("ESP-NOW Text from ");
            Serial.print(mac_str);
            Serial.print(": ");
            Serial.println(text);
            break;
        }
        
        case ESPNOW_MSG_GPS_DATA: {
            Serial.print("ESP-NOW GPS data from ");
            Serial.println(mac_str);
            // Parse and use GPS data if needed
            break;
        }
        
        case ESPNOW_MSG_TELEMETRY: {
            Serial.print("ESP-NOW Telemetry from ");
            Serial.println(mac_str);

            // Extract telemetry data from message
            memcpy(&dataFromGci, item.message.data, sizeof(structMsgFromGci));

            // Update individual variables for compatibility
            modeHeadLights = dataFromGci.modeLights;
            outdoorLuminosity = dataFromGci.outdoorLum;
            airTemperature = dataFromGci.airTemp + temperature_adj;  // Apply temperature offset
            battVoltage = dataFromGci.battVolts;
            fuelLevel = dataFromGci.fuel;

            Serial.printf("Telemetry: Lights=%d, Lum=%d, Temp=%.1f, Batt=%.2f, Fuel=%.1f\n",
                         modeHeadLights, outdoorLuminosity, airTemperature, battVoltage, fuelLevel);
            break;
        }
        
        case ESPNOW_MSG_COMMAND: {
            Serial.print("ESP-NOW Command from ");
            Serial.println(mac_str);
            // Handle commands
            break;
        }

        case ESPNOW_MSG_ACK: {
            Serial.printf("ESP-NOW ACK from %s - GCI paired successfully!\n", mac_str);

            // Add GCI as peer if not already added
            if (!isPeerRegistered(item.mac_addr)) {
                if (addPeer(item.mac_addr, "GCI")) {
                    Serial.println("Added GCI to peer list - communication established");
                } else {
                    Serial.println("Failed to add GCI as peer");
                }
            } else {
                Serial.println("GCI already registered as peer");
            }

            // Save peer MAC to EEPROM when ACK is received during pairing
            // Update variable directly without triggering ESP-NOW restart
            if (espnow_gci_mac_addr != String(mac_str)) {
                espnow_gci_mac_addr = String(mac_str);
                Serial.printf("Saved GCI MAC address: %s\n", mac_str);
                // Variable change will be detected by system_task and saved to EEPROM
            }

            // Close the pairing window now that peer is added
            espnow_pair_gci = false;
            set_var_espnow_pair_gci(false);

            break;
        }

        case ESPNOW_MSG_HEARTBEAT: {
            #if DEBUG_ESPNOW == 1
            Serial.print("ESP-NOW Heartbeat from ");
            Serial.println(mac_str);
            #endif
            break;
        }
    }
}

// Callback functions
void espnowOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    #if DEBUG_ESPNOW == 1
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.println("ESP-NOW: Send failed");
    } else {
        Serial.println("ESP-NOW: Send success");
    }
    #endif
}

void espnowOnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    // Accept wrapped messages (header is 9 bytes minimum)
    if (data_len >= ESPNOW_PACKET_HEADER_SIZE) {
        // Filter wrapped messages - only accept from registered peers
        bool is_known_peer = false;
        for (int i = 0; i < espNow.getPeerCount(); i++) {
            espnow_peer_info_t* peer = espNow.getPeerInfo(i);
            if (peer && memcmp(peer->mac_addr, mac_addr, 6) == 0) {
                is_known_peer = true;
                break;
            }
        }

        if (!is_known_peer) {
            // Special case: Accept ACK messages from unknown peers during pairing window
            // (espnow_pair_gci will be true for a short time after pairing is initiated)
            espnow_message_t* msg = (espnow_message_t*)data;
            if (msg->type == ESPNOW_MSG_ACK && espnow_pair_gci) {
                // This is an ACK response to our pairing request - allow it
                Serial.println("ESP-NOW: Accepting ACK from unknown peer during pairing");
            } else {
                Serial.println("ESP-NOW: Ignoring message from unknown peer");
                return;
            }
        }

        espnow_recv_item_t item;
        memcpy(item.mac_addr, mac_addr, 6);

        // Copy the received data (variable size, not full structure)
        memcpy(&item.message, data, data_len);
        item.rssi = -50; // Default value if RSSI not available

        // Queue for processing
        if (espnowRecvQueue != NULL) {
            xQueueSend(espnowRecvQueue, &item, 0);
        }
    } else {
        // Too small to be a valid wrapped message
        Serial.printf("ESP-NOW: Received message too small: %d bytes\n", data_len);
    }
}