#include "tasks.h"
#include "config.h"
#include "globals.h"

void createAllTasks() {
    // Core 1 tasks first — all block immediately on queues/serial so they
    // don't compete for heap during gui_task's first render.
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

    // Core 0: espnow_task blocks on firstRenderDone semaphore before touching WiFi,
    // so it cannot call esp_wifi_start() until after gui_task's first render.
    xTaskCreatePinnedToCore(
        espnowTask,
        "ESP-NOW Task",
        ESPNOW_TASK_STACK_SIZE,
        NULL,
        ESPNOW_TASK_PRIORITY,
        &espnowTaskHandle,
        0  // Core 0 for WiFi operations
    );

    // gui_task is created last so all task stacks are already allocated when
    // the first lv_timer_handler() call runs — eliminates concurrent heap
    // contention from task creation during the first LVGL render.
    xTaskCreatePinnedToCore(
        guiTask,
        "GUI Task",
        GUI_TASK_STACK_SIZE,
        NULL,
        GUI_TASK_PRIORITY,
        &guiTaskHandle,
        0
    );
    
#if DEBUG_INIT == 1
    Serial.println("All FreeRTOS tasks created");
#endif
}