#include "system_task.h"
#include "config.h"
#include "globals.h"
#include "types.h"
#include "hardware/display.h"
#include "storage/preferences_manager.h"
#include "utils/sleep_manager.h"
#include "communication/meshtastic_admin.h"

void systemTask(void *parameter) {
    while (true) {
        // Initialize GPS config after Meshtastic connection (polled approach to avoid stack overflow in callback)
        initGpsConfigOnBoot();

        // Check sleep pin status (highest priority - check first)
        if (shouldEnterSleep()) {
            enterDeepSleep();
            // Device will reboot from setup() on wake - code never returns here
        }

        // Handle preferences reset
        if (reset_preferences == true) {
            reset_preferences = false;  // Reset flag before clearing to prevent restart loop
            clearAllPreferences();
        }

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
        
        if (espnow_gci_mac_addr != old_espnow_gci_mac_addr) {
            eepromWriteItem_t item;
            item.type = EEPROM_STRING;
            strcpy(item.key, "gci_mac");  // Shortened key for NVS 15-char limit
            strcpy(item.value.stringVal, espnow_gci_mac_addr.c_str());
            xQueueSend(eepromWriteQueue, &item, 0);

            old_espnow_gci_mac_addr = espnow_gci_mac_addr;
        }

        if (speaker_volume != old_speaker_volume) {
            eepromWriteItem_t item;
            item.type = EEPROM_INT;
            strcpy(item.key, "speaker_volume");
            item.value.intVal = speaker_volume;
            xQueueSend(eepromWriteQueue, &item, 0);

            old_speaker_volume = speaker_volume;
        }

        if (temperature_adj != old_temperature_adj) {
            eepromWriteItem_t item;
            item.type = EEPROM_FLOAT;
            strcpy(item.key, "temperature_adj");
            item.value.floatVal = temperature_adj;
            xQueueSend(eepromWriteQueue, &item, 0);

            old_temperature_adj = temperature_adj;
        }

        vTaskDelay(pdMS_TO_TICKS(100));  // Check sleep pin frequently (was 1000ms)
    }
}