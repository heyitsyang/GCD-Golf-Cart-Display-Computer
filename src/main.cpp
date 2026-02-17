/********************************************************************************************
*    Golf Cart Display (GCD) - RTOS Version (Modularized with ESP-NOW)                      *
*                                                                                           *
*    Refactored to use FreeRTOS tasks for better concurrent processing                      *
*    Maintains original UART0 split configuration (RX=GPS, TX=Debug)                        *
*    Dedicated Meshtastic callback task to prevent stack overflow                           *
*    CONVERTED TO USE NEOGPS LIBRARY FOR GNSS COMPATIBILITY                                 *
*    MODULARIZED for better maintainability and organization                                *
*    INTEGRATED ESP-NOW for direct device-to-device communication                           *
*                                                                                           *
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
#include "communication/meshtastic_admin.h"


// Tasks
#include "tasks/tasks.h"
#include "tasks/meshtastic_callback_task.h"

// UI
#include "ui_eez/ui.h"
#include "ui_eez/actions.h"
#include "ui/venue_event_display.h"
#include "ui/espnow_display.h"
#include "ui/tone_actions.h"

// Utils
#include "utils/time_utils.h"
#include "utils/sleep_manager.h"

// Function prototypes
#include "prototypes.h"
// Note: get_set_vars.h is now included via globals.h


/*****************
 *     SETUP     *
 *****************/

void setup() {
    // Initialize Serial for debug output only (TX on pin 1)
    Serial.begin(GPS_BAUD, SERIAL_8N1, 3, 1);  // RX=3 (GPS), TX=1 (debug)

    // Disable Serial RX to prevent conflicts
    Serial.end();
    Serial.begin(GPS_BAUD, SERIAL_8N1, 3, 1);

    // Seed PRNG with hardware RNG so packet IDs are unique across reboots
    randomSeed(esp_random());

    // Print version and MAC (always shown - essential boot info)
    version = String('v') + String(VERSION);
    cyd_mac_addr = String(WiFi.macAddress());
    Serial.printf("\nGCD %s | MAC: %s\n", version.c_str(), cyd_mac_addr.c_str());

    // Initialize storage and load preferences
    initPreferences();
    loadPreferences();

    // Initialize display
    initDisplay();

    // Initialize backlight
    initBacklight();

    // Initialize display and touchscreen
    initTouchscreen();

    // Initialize speaker
    initSpeaker();

    // Initialize sleep pin
    initSleepPin();

    // Initialize sleep mode state machine (after loadPreferences to use backlight_timeout)
    initSleepModeStateMachine();

    // Startup tone
    tone_startup();

    // Initialize LVGL
#if DEBUG_INIT == 1
    Serial.printf("LVGL v%d.%d.%d\n", lv_version_major(), lv_version_minor(), lv_version_patch());
#endif

    // Clear screen to black immediately after LVGL init
    lv_obj_t* default_screen = lv_scr_act();
    lv_obj_set_style_bg_color(default_screen, lv_color_black(), LV_PART_MAIN);
    lv_refr_now(NULL);  // Force immediate refresh

    // Initialize display strings
    cur_date = String("NO GPS");
    cur_temp = String("--");
    wx_rcv_time = String("        NO DATA YET");
    np_rcv_time = String("        NO DATA YET");
    espnow_status = "Not initialized";
    espnow_last_received = "";

    // Initialize UI from EEZ Studio
    ui_init();
    
    // Initialize Meshtastic
    mt_serial_init(MT_SERIAL_RX_PIN, MT_SERIAL_TX_PIN, MT_DEV_BAUD_RATE);
    randomSeed(micros());
    mt_request_node_report(connected_callback);
    set_text_message_callback(text_message_callback);
    set_portnum_callback(admin_portnum_callback);
    
    // Initialize application variables
    manual_reboot = false;
    new_rx_data_flag = false;
    mesh_serial_enabled = true;
    reset_preferences = false;
    espnow_pair_gci = false;

    // Create synchronization objects
    gpsMutex = xSemaphoreCreateMutex();
    eepromMutex = xSemaphoreCreateMutex();
    displayMutex = xSemaphoreCreateMutex();
    hotPacketMutex = xSemaphoreCreateMutex();  // Protects weather and venue/event data
    eepromWriteQueue = xQueueCreate(10, sizeof(eepromWriteItem_t));
    meshtasticCallbackQueue = xQueueCreate(30, sizeof(meshtasticCallbackItem_t));  // Matches radio's ~30 packet buffer
    espnowRecvQueue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_recv_item_t));
    gpsConfigCallbackQueue = xQueueCreate(2, sizeof(gpsConfigCallbackItem_t));
    
    // Create all FreeRTOS tasks (including ESP-NOW)
    createAllTasks();

    Serial.println("Ready");
}

/*****************
 *     LOOP      *
 *****************/

void loop() {
    // Main loop is handled by FreeRTOS tasks
    vTaskDelay(pdMS_TO_TICKS(1000));
}