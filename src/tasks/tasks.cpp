#include "tasks.h"
#include "config.h"
#include "globals.h"

void createAllTasks() {
    xTaskCreatePinnedToCore(
        gpsTask,
        "GPS Task",
        GPS_TASK_STACK_SIZE,
        NULL,
        GPS_TASK_PRIORITY,
        &gpsTaskHandle,
        1
    );
    
    xTaskCreatePinnedToCore(
        guiTask,
        "GUI Task",
        GUI_TASK_STACK_SIZE,
        NULL,
        GUI_TASK_PRIORITY,
        &guiTaskHandle,
        0
    );
    
    xTaskCreatePinnedToCore(
        meshtasticTask,
        "Meshtastic Task",
        MESHTASTIC_TASK_STACK_SIZE,
        NULL,
        MESHTASTIC_TASK_PRIORITY,
        &meshtasticTaskHandle,
        1
    );
    
    xTaskCreatePinnedToCore(
        meshtasticCallbackTask,
        "Meshtastic Callback Task",
        MESHTASTIC_CALLBACK_TASK_STACK_SIZE,
        NULL,
        MESHTASTIC_CALLBACK_TASK_PRIORITY,
        &meshtasticCallbackTaskHandle,
        1
    );
    
    xTaskCreatePinnedToCore(
        espnowTask,
        "ESP-NOW Task",
        ESPNOW_TASK_STACK_SIZE,
        NULL,
        ESPNOW_TASK_PRIORITY,
        &espnowTaskHandle,
        0  // Core 0 for WiFi operations
    );
    
    xTaskCreatePinnedToCore(
        eepromTask,
        "EEPROM Task",
        EEPROM_TASK_STACK_SIZE,
        NULL,
        EEPROM_TASK_PRIORITY,
        &eepromTaskHandle,
        1
    );
    
    xTaskCreatePinnedToCore(
        systemTask,
        "System Task",
        SYSTEM_TASK_STACK_SIZE,
        NULL,
        SYSTEM_TASK_PRIORITY,
        &systemTaskHandle,
        1
    );
    
    Serial.println("All FreeRTOS tasks created successfully");
}