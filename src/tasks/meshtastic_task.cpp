#include "meshtastic_task.h"
#include "config.h"
#include "globals.h"
#include "Meshtastic.h"

void meshtasticTask(void *parameter) {
    while (true) {
        // Handle mesh communication enable/disable
        if (mesh_comm != old_mesh_comm) {
            old_mesh_comm = mesh_comm;
            if (mesh_comm == false) {
                mt_serial_end();
                Serial.println("meshSerial disabled");
            } else {
                mt_serial_init(MT_SERIAL_RX_PIN, MT_SERIAL_TX_PIN, MT_DEV_BAUD_RATE);
                Serial.println("meshSerial enabled");
            }
        }
        
        uint32_t now = millis();
        bool can_send = mt_loop(now);
        
        // Send periodic test message
        if (can_send && now >= next_send_time) {
            Serial.print("\nSending test message at: ");
            Serial.println(hhmm_str);
            
            uint32_t dest = BROADCAST_ADDR;
            uint8_t channel_index = 0;
            
            bool success = mt_send_text("Hello, world from the CYD Golf Cart Computer!", 
                                       dest, channel_index);
            Serial.print("mt_send_text returned: ");
            Serial.println(success ? "SUCCESS" : "***** FAILED ****");
            
            next_send_time = now + SEND_PERIOD * 1000;
            Serial.printf("Next send time in: %d minutes\n", (SEND_PERIOD / 60));
        }
        
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}