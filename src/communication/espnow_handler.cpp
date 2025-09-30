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
    if (!initialized || len > ESPNOW_MAX_DATA_LEN - sizeof(espnow_message_t)) {
        return false;
    }
    
    espnow_message_t msg;
    msg.type = type;
    msg.timestamp = millis();
    msg.msg_id = getNextMessageId();
    msg.data_len = len;
    memcpy(msg.data, data, len);
    
    return sendRawData(mac_addr, (uint8_t*)&msg, sizeof(msg));
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
            if (!sendRawData(peers[i].mac_addr, data, len)) {
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

bool ESPNowHandler::sendGolfCartCommand(const uint8_t *mac_addr, int cmdNumber) {
    if (!initialized) {
        return false;
    }

    // Prepare command data in golf cart format
    dataToGci.cmdNumber = cmdNumber;
    cmdToGci = cmdNumber;  // Update global variable

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
            break;
        }
        
        case ESPNOW_MSG_COMMAND: {
            Serial.print("ESP-NOW Command from ");
            Serial.println(mac_str);
            // Handle commands
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

// Handle raw golf cart interface data
void handleRawGolfCartData(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    // Update peer info for timeout tracking
    for (int i = 0; i < espNow.getPeerCount(); i++) {
        espnow_peer_info_t* peer = espNow.getPeerInfo(i);
        if (peer && memcmp(peer->mac_addr, mac_addr, 6) == 0) {
            peer->is_online = true;
            peer->last_seen = millis();
            peer->last_rssi = -50;
            break;
        }
    }

    // Only set connected status if this MAC is in our peer list
    bool is_configured_peer = false;
    for (int i = 0; i < espNow.getPeerCount(); i++) {
        espnow_peer_info_t* peer = espNow.getPeerInfo(i);
        if (peer && memcmp(peer->mac_addr, mac_addr, 6) == 0) {
            is_configured_peer = true;
            break;
        }
    }

    // Set connected status when we receive data from a configured peer
    if (is_configured_peer && !espnow_connected) {
        espnow_connected = true;
        set_var_espnow_connected(true);
        Serial.printf("*** ESP-NOW connection established ***\n");
    }

    // Copy received data to global golf cart variables
    memcpy(&dataFromGci, data, sizeof(structMsgFromGci));

    // Update individual variables for compatibility
    modeHeadLights = dataFromGci.modeLights;
    outdoorLuminosity = dataFromGci.outdoorLum;
    airTemperature = dataFromGci.airTemp;
    battVoltage = dataFromGci.battVolts;
    fuelLevel = dataFromGci.fuel;

    // Log received data
    char mac_str[18];
    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac_addr[0], mac_addr[1], mac_addr[2],
            mac_addr[3], mac_addr[4], mac_addr[5]);

    Serial.printf("Golf Cart Data from %s: Lights=%d, Lum=%d, Temp=%.1f, Batt=%.2f, Fuel=%.1f\n",
                  mac_str, modeHeadLights, outdoorLuminosity, airTemperature, battVoltage, fuelLevel);
}

// Handle raw golf cart command data
void handleRawGolfCartCommand(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    // Update peer info for timeout tracking
    for (int i = 0; i < espNow.getPeerCount(); i++) {
        espnow_peer_info_t* peer = espNow.getPeerInfo(i);
        if (peer && memcmp(peer->mac_addr, mac_addr, 6) == 0) {
            peer->is_online = true;
            peer->last_seen = millis();
            peer->last_rssi = -50;
            break;
        }
    }

    // Only set connected status if this MAC is in our peer list
    bool is_configured_peer = false;
    for (int i = 0; i < espNow.getPeerCount(); i++) {
        espnow_peer_info_t* peer = espNow.getPeerInfo(i);
        if (peer && memcmp(peer->mac_addr, mac_addr, 6) == 0) {
            is_configured_peer = true;
            break;
        }
    }

    // Set connected status when we receive data from a configured peer
    if (is_configured_peer && !espnow_connected) {
        espnow_connected = true;
        set_var_espnow_connected(true);
        Serial.printf("*** ESP-NOW connection established ***\n");
    }

    structMsgToGci receivedCmd;
    memcpy(&receivedCmd, data, sizeof(structMsgToGci));

    // Log received command
    char mac_str[18];
    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac_addr[0], mac_addr[1], mac_addr[2],
            mac_addr[3], mac_addr[4], mac_addr[5]);

    Serial.printf("Golf Cart Command from %s: cmd=%d\n", mac_str, receivedCmd.cmdNumber);

    // Process the command if needed
    // (Add command processing logic here based on your needs)
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
    // First check if this MAC address is in our configured peer list
    bool is_configured_peer = false;
    for (int i = 0; i < espNow.getPeerCount(); i++) {
        espnow_peer_info_t* peer = espNow.getPeerInfo(i);
        if (peer && memcmp(peer->mac_addr, mac_addr, 6) == 0) {
            is_configured_peer = true;
            break;
        }
    }

    // If no peers are configured, or this sender is not a configured peer, ignore the data
    if (espNow.getPeerCount() == 0 || !is_configured_peer) {
        return; // Silently drop data from unconfigured peers
    }
    // Check if this is a raw golf cart telemetry message (from golf cart internal)
    if (data_len == sizeof(structMsgFromGci)) {
        // Handle raw golf cart interface data
        handleRawGolfCartData(mac_addr, data, data_len);
        return;
    }

    // Check if this is a raw golf cart command message (from another display)
    if (data_len == sizeof(structMsgToGci)) {
        // Handle raw golf cart command data
        handleRawGolfCartCommand(mac_addr, data, data_len);
        return;
    }

    // Handle wrapped ESP-NOW messages (existing protocol)
    espnow_recv_item_t item;
    memcpy(item.mac_addr, mac_addr, 6);

    // Safely copy message data
    size_t copy_len = min(data_len, (int)sizeof(espnow_message_t));
    memcpy(&item.message, data, copy_len);

    // Try to get RSSI (this is approximate)
    item.rssi = -50; // Default value if RSSI not available

    // Queue for processing
    if (espnowRecvQueue != NULL) {
        xQueueSend(espnowRecvQueue, &item, 0);
    }
}