#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>
#include "config.h"

// EEPROM Write Queue Item
typedef enum {
    EEPROM_FLOAT,
    EEPROM_INT,
    EEPROM_STRING,
    EEPROM_BOOL
} eepromType_t;

typedef struct {
    eepromType_t type;
    char key[32];
    union {
        float floatVal;
        int intVal;
        char stringVal[64];
        bool boolVal;
    } value;
} eepromWriteItem_t;

// Meshtastic callback item
typedef struct {
    uint32_t from;
    uint32_t to;
    uint8_t channel;
    char text[MAX_MESHTASTIC_PAYLOAD];
} meshtasticCallbackItem_t;

// Hot Packet Types
enum HotPacketType {
    HOT_PACKET_WEATHER = 1,
    HOT_PACKET_VENUE_EVENT = 2
};

// ESP-NOW message types
typedef enum {
    ESPNOW_MSG_TEXT = 0,
    ESPNOW_MSG_GPS_DATA = 1,
    ESPNOW_MSG_TELEMETRY = 2,
    ESPNOW_MSG_COMMAND = 3,
    ESPNOW_MSG_ACK = 4,
    ESPNOW_MSG_HEARTBEAT = 5
} espnow_msg_type_t;

// ESP-NOW message structure
typedef struct {
    uint8_t type;
    uint32_t timestamp;
    uint16_t msg_id;
    uint8_t data[ESPNOW_MAX_DATA_LEN];
    uint16_t data_len;
} espnow_message_t;

// ESP-NOW queue item for received messages
typedef struct {
    uint8_t mac_addr[6];
    espnow_message_t message;
    int rssi;
} espnow_recv_item_t;

// ESP-NOW peer info
typedef struct {
    uint8_t mac_addr[6];
    char name[32];
    bool is_online;
    uint32_t last_seen;
    int last_rssi;
} espnow_peer_info_t;

// Golf cart interface message structures
typedef struct struct_msg_from_gci {
    int modeLights;
    int outdoorLum;
    float airTemp;
    float battVolts;
    float fuel;
} structMsgFromGci;

typedef struct struct_msg_to_gci {
    int cmdNumber;
} structMsgToGci;

#endif // TYPES_H