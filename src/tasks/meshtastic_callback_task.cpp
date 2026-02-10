#include "meshtastic_callback_task.h"
#include "config.h"
#include "globals.h"
#include "types.h"
#include "communication/hot_packet_parser.h"
#include "Meshtastic.h"

void meshtasticCallbackTask(void *parameter) {
    meshtasticCallbackItem_t item;
    
    while (true) {
        if (xQueueReceive(meshtasticCallbackQueue, &item, portMAX_DELAY)) {
#if DEBUG_GCM_MESSAGES
            Serial.printf("GCM RX: ch=%d from=%lu to=%lu msg='%s'\n",
                         item.channel, item.from, item.to, item.text);
            if (item.to == 0xFFFFFFFF) {
                Serial.println("  (BROADCAST)");
            } else if (item.to == my_node_num) {
                Serial.println("  (DM to me)");
            } else {
                Serial.println("  (DM to someone else)");
            }
#endif
            
            // Check for HoT packet
            if (isHotPacket(item.text)) {
                processHotPacket(item.text);
            }
        }
    }
}

void connected_callback(mt_node_t *node, mt_nr_progress_t progress) {
    if (not_yet_connected) {
        Serial.println("Connected to Meshtastic device!");
        not_yet_connected = false;
        // GPS config init will be handled by system task polling
    }
}

void text_message_callback(uint32_t from, uint32_t to, uint8_t channel, const char *text) {
#if DEBUG_GCM_MESSAGES
    Serial.printf("GCM RX callback: from=%lu, to=%lu, ch=%d, text='%s'\n",
                 from, to, channel, text ? text : "NULL");
#endif

    meshtasticCallbackItem_t item;
    item.from = from;
    item.to = to;
    item.channel = channel;
    
    if (text != NULL) {
        strncpy(item.text, text, MAX_MESHTASTIC_PAYLOAD - 1);
        item.text[MAX_MESHTASTIC_PAYLOAD - 1] = '\0';
    } else {
        item.text[0] = '\0';
    }
    
    if (xQueueSend(meshtasticCallbackQueue, &item, 0) != pdTRUE) {
        Serial.println("Warning: Meshtastic callback queue full, message dropped");
    }
}