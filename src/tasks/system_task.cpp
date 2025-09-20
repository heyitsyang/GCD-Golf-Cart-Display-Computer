#include "system_task.h"
#include "config.h"
#include "globals.h"
#include "types.h"
#include "hardware/display.h"

void systemTask(void *parameter) {
    while (true) {
        // Handle manual reboot
        if (manual_reboot == true) {
            ESP.restart();
        }
        
        // Write values to eeprom only if changed
        if (day_backlight != old_day_backlight) {
            setBacklight((day_backlight * 20) + 55);
            
            eepromWriteItem_t item;
            item.type = EEPROM_INT;
            strcpy(item.key, "day_backlight");
            item.value.intVal = day_backlight;
            xQueueSend(eepromWriteQueue, &item, 0);
            
            old_day_backlight = day_backlight;
        }
        
        if (night_backlight != old_night_backlight) {
            setBacklight(night_backlight * 20);
            
            eepromWriteItem_t item;
            item.type = EEPROM_INT;
            strcpy(item.key, "night_backlight");
            item.value.intVal = night_backlight;
            xQueueSend(eepromWriteQueue, &item, 0);
            
            old_night_backlight = night_backlight;
        }
        
        if (max_hdop != old_max_hdop) {
            eepromWriteItem_t item;
            item.type = EEPROM_FLOAT;
            strcpy(item.key, "max_hdop");
            item.value.floatVal = max_hdop;
            xQueueSend(eepromWriteQueue, &item, 0);
            
            old_max_hdop = max_hdop;
        }
        
        if (espnow_mac_addr != old_espnow_mac_addr) {
            eepromWriteItem_t item;
            item.type = EEPROM_STRING;
            strcpy(item.key, "espnow_mac_addr");
            strcpy(item.value.stringVal, espnow_mac_addr.c_str());
            xQueueSend(eepromWriteQueue, &item, 0);
            
            old_espnow_mac_addr = espnow_mac_addr;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}