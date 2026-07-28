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

    // Golf cart interface - raw message sending
    bool sendGolfCartCommand(const uint8_t *mac_addr, gci_command_t cmdNumber, const void *payload = nullptr, size_t payloadSize = 0);

    // Status messages to GCI
    bool sendIsHome(bool is_home);
    bool sendIsDaytime(bool is_daytime);
    bool sendFuelConfig();
    
    // Status
    bool isInitialized() { return initialized; }
    String getMyMacAddress();
    String getStatus() { return status; }

    // Management
    bool restart();
    
    // Message handling
    void processReceivedMessage(espnow_recv_item_t &item);
    
private:
    bool initialized = false;
    uint16_t next_msg_seq_num = 0;
    espnow_peer_info_t peers[ESPNOW_MAX_PEER_NUM];
    int peer_count = 0;
    String status = "Not initialized";
    
    uint16_t getNextMsgSeqNum() { return next_msg_seq_num++; }
    bool sendRawData(const uint8_t *mac_addr, const uint8_t *data, size_t len);

public:  // Make public so espnow_task can use it
    bool macStringToBytes(const String& mac_str, uint8_t* mac_bytes);
};

// Global instance
extern ESPNowHandler espNow;

// Forgets the paired GCI. Counterpart to the pairing window opened by
// espnow_pair_gci; called from the Settings 2 PAIR GCI long press.
void espnowUnpairGci();

// Callback functions
void espnowOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void espnowOnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len);

#endif // ESPNOW_HANDLER_H