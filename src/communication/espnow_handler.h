#ifndef ESPNOW_HANDLER_H
#define ESPNOW_HANDLER_H

#include <esp_now.h>
#include <WiFi.h>
#include "types.h"

class ESPNowHandler {
public:
    bool init();
    void deinit();
    
    // Peer management
    bool addPeer(const uint8_t *mac_addr, const char* name = nullptr);
    bool addPeerFromString(const String& mac_str, const char* name = nullptr);
    bool removePeer(const uint8_t *mac_addr);
    bool isPeerRegistered(const uint8_t *mac_addr);
    int getPeerCount() { return peer_count; }
    espnow_peer_info_t* getPeerInfo(int index);
    
    // Message sending
    bool sendMessage(const uint8_t *mac_addr, espnow_msg_type_t type, 
                     const uint8_t *data, size_t len);
    bool sendTextMessage(const uint8_t *mac_addr, const String& text);
    bool broadcast(espnow_msg_type_t type, const uint8_t *data, size_t len);
    bool broadcastText(const String& text);
    
    // Status
    bool isInitialized() { return initialized; }
    String getMyMacAddress();
    String getStatus() { return status; }
    
    // Message handling
    void processReceivedMessage(espnow_recv_item_t &item);
    
private:
    bool initialized = false;
    uint16_t next_msg_id = 0;
    espnow_peer_info_t peers[ESPNOW_MAX_PEER_NUM];
    int peer_count = 0;
    String status = "Not initialized";
    
    uint16_t getNextMessageId() { return next_msg_id++; }
    bool sendRawData(const uint8_t *mac_addr, const uint8_t *data, size_t len);
    bool macStringToBytes(const String& mac_str, uint8_t* mac_bytes);
};

// Global instance
extern ESPNowHandler espNow;

// Callback functions
void espnowOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void espnowOnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len);

#endif // ESPNOW_HANDLER_H