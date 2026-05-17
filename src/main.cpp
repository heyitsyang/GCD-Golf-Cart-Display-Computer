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
#include "storage/favorites.h"

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
#include "ui/chat_screen.h"
#include "ui/canned_screen.h"

// Utils
#include "utils/time_utils.h"
#include "utils/sleep_manager.h"

// Function prototypes
#include "prototypes.h"
// Note: get_set_vars.h is now included via globals.h

#if DEBUG_HEAP
  #define HEAP_LOG(label) \
      Serial.printf("[HEAP] %-26s free=%u, largest=%u\n", \
                    (label), (unsigned)ESP.getFreeHeap(), \
                    (unsigned)ESP.getMaxAllocHeap())

static void heapAllocFailedCallback(size_t size, uint32_t caps, const char *func) {
    Serial.printf("[HEAP FAIL] size=%u caps=0x%x in %s | free=%u largest=%u\n",
                  (unsigned)size, (unsigned)caps, func,
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}
#else
  #define HEAP_LOG(label) ((void)0)
#endif


/*****************
 *     SETUP     *
 *****************/

void setup() {
    // Initialize Serial for debug output only (TX on pin 1)
    Serial.begin(GPS_BAUD, SERIAL_8N1, 3, 1);  // RX=3 (GPS), TX=1 (debug)

    // Disable Serial RX to prevent conflicts
    Serial.end();
    Serial.begin(GPS_BAUD, SERIAL_8N1, 3, 1);

#if DEBUG_HEAP
    heap_caps_register_failed_alloc_callback(heapAllocFailedCallback);
#endif

    // Check for spurious wake before any peripheral init.
    // If woken by EXT0 (SLEEP_PIN) but pin is already LOW, glitch woke us — go back to sleep.
    checkSpuriousWake();

    // Seed PRNG with hardware RNG so packet IDs are unique across reboots
    randomSeed(esp_random());

    // Print version and MAC (always shown - essential boot info)
    version = String('v') + String(VERSION);
    cyd_mac_addr = String(WiFi.macAddress());
    Serial.printf("\nGCD %s | MAC: %s\n", version.c_str(), cyd_mac_addr.c_str());
    HEAP_LOG("start of setup");

    // Initialize storage and load preferences
    initPreferences();
    loadPreferences();
    favoritesLoad();
    HEAP_LOG("after loadPreferences");

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
    HEAP_LOG("after hw init");

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
    strncpy(hotPacketBuffer_np_rcv_time[0], "        NO DATA YET", HP_RCV_TIME_SIZE - 1);
    espnow_status = "Not initialized";
    espnow_last_received = "";

    // Initialize UI from EEZ Studio
    ui_init();
    HEAP_LOG("after ui_init");

    // Initialize chat / messaging UI (mounts hand-coded LVGL into the
    // EEZ-defined containers; must run AFTER ui_init() so the
    // objects.* widgets exist).
    chatScreenInit();
    HEAP_LOG("after chatScreenInit");
    cannedScreenInit();
    HEAP_LOG("after cannedScreenInit");
    // Initialize Meshtastic
    mt_serial_init(MT_SERIAL_RX_PIN, MT_SERIAL_TX_PIN, MT_DEV_BAUD_RATE);
    randomSeed(micros());
    mt_request_node_report(connected_callback);
    set_text_message_callback(text_message_callback);
    set_portnum_callback(admin_portnum_callback);
    HEAP_LOG("after mt_serial_init");

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
    chatBufferMutex = xSemaphoreCreateMutex();  // Protects chat ring buffer
    eepromWriteQueue = xQueueCreate(10, sizeof(eepromWriteItem_t));
    meshtasticCallbackQueue = xQueueCreate(30, sizeof(meshtasticCallbackItem_t));  // Matches radio's ~30 packet buffer
    espnowRecvQueue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_recv_item_t));
    gpsConfigCallbackQueue = xQueueCreate(2, sizeof(gpsConfigCallbackItem_t));
    chatTxQueue = xQueueCreate(8, sizeof(chatTxItem_t));
    firstRenderDone = xSemaphoreCreateBinary();  // espnow_task waits on this before WiFi init
    HEAP_LOG("after mutex/queue create");

    // Restore unread DMs from previous GCI sleep cycle (no-op on first boot or non-GCI use).
    // Must run after chatBufferMutex is created and after initSpeaker() so beeps work.
    loadDmsFromNvs();
    HEAP_LOG("after loadDmsFromNvs");

    // Create all FreeRTOS tasks (including ESP-NOW)
    createAllTasks();
    HEAP_LOG("after createAllTasks");

    Serial.println("Ready");
}

/*****************
 *     LOOP      *
 *****************/

void loop() {
    // Main loop is handled by FreeRTOS tasks
    vTaskDelay(pdMS_TO_TICKS(1000));
}