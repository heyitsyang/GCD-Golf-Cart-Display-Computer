/********************************************************************************************
*    CYD-EEZ-LVGL Template - RTOS Version (Modularized with ESP-NOW)                        *
*                                                                                            *
*    Refactored to use FreeRTOS tasks for better concurrent processing                      *
*    Maintains original UART0 split configuration (RX=GPS, TX=Debug)                        *
*    Dedicated Meshtastic callback task to prevent stack overflow                           *
*    CONVERTED TO USE NEOGPS LIBRARY FOR GNSS COMPATIBILITY                                 *
*    MODULARIZED for better maintainability and organization                               *
*    INTEGRATED ESP-NOW for direct device-to-device communication                           *
*                                                                                            *
********************************************************************************************/

#include <Arduino.h>
#include <WiFi.h>

// Configuration and types
#include "config.h"
#include "types.h"
#include "globals.h"  // This now includes get_set_vars.h
#include "version.h"

// Time library after our includes to avoid conflicts
#include <TimeLib.h>

// Hardware modules
#include "hardware/display.h"

// Storage
#include "storage/preferences_manager.h"

// Communication
#include "Meshtastic.h"
#include "communication/hot_packet_parser.h"
#include "communication/espnow_handler.h"

// Tasks
#include "tasks/tasks.h"
#include "tasks/meshtastic_callback_task.h"

// UI
#include "ui/ui.h"
#include "ui/actions.h"
#include "ui/venue_event_display.h"
#include "ui/espnow_display.h"

// Utils
#include "utils/time_utils.h"

// Function prototypes
#include "prototypes.h"
// Note: get_set_vars.h is now included via globals.h

/*****************
 *     SETUP     *
 *****************/

void setup() {
    // Initialize Serial/GPS (UART0 split design)
    Serial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    Serial.print("\ngpsSerial started at 9600 baud rate");
    
    // Print version
    version = String('v') + String(VERSION);
    Serial.println("\nSW " + version);
    Serial.println("With ESP-NOW Integration");
    
    // Get MAC address
    cyd_mac_addr = String(WiFi.macAddress());
    Serial.println("MAC: " + cyd_mac_addr);
    
    // Initialize storage and load preferences
    initPreferences();
    loadPreferences();
    
    // Initialize GPS
    avgAzimuthDeg.begin();
    avgSpeed.begin();

    // Initialize display
    initDisplay();
    
    // Initialize backlight
    initBacklight();
    
    // Initialize display and touchscreen
    initTouchscreen();
    
    // Initialize LVGL
    String LVGL_Arduino = "LVGL v" + String(lv_version_major()) + "." + 
                         String(lv_version_minor()) + "." + String(lv_version_patch());
    Serial.println(LVGL_Arduino);
    
    // Initialize display strings
    cur_date = String("NO GPS");
    cur_temp = String("--");
    wx_rcv_time = String(" NO DATA YET");
    np_rcv_time = String(" NO DATA YET");
    espnow_status = "Not initialized";
    espnow_last_received = "";
    

    
    // Initialize UI from EEZ Studio
    ui_init();
    
    // Initialize Meshtastic
    mt_serial_init(MT_SERIAL_RX_PIN, MT_SERIAL_TX_PIN, MT_DEV_BAUD_RATE);
    randomSeed(micros());
    mt_request_node_report(connected_callback);
    set_text_message_callback(text_message_callback);
    
    // Initialize application variables
    manual_reboot = false;
    mesh_comm = true;
    
    // Create synchronization objects
    gpsMutex = xSemaphoreCreateMutex();
    eepromMutex = xSemaphoreCreateMutex();
    displayMutex = xSemaphoreCreateMutex();
    eepromWriteQueue = xQueueCreate(10, sizeof(eepromWriteItem_t));
    meshtasticCallbackQueue = xQueueCreate(5, sizeof(meshtasticCallbackItem_t));
    // Note: espnowRecvQueue is created in espnowTask
    
    // Create all FreeRTOS tasks (including ESP-NOW)
    createAllTasks();
    
    Serial.println("Setup complete - System ready");
}

/*****************
 *     LOOP      *
 *****************/

void loop() {
    // Main loop is handled by FreeRTOS tasks
    vTaskDelay(pdMS_TO_TICKS(1000));
}