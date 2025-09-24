#ifndef GLOBALS_H
#define GLOBALS_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <XPT2046_Touchscreen.h>
#include <NMEAGPS.h>
#include <Timezone.h>
#include <movingAvg.h>
#include <Preferences.h>
#include <JC_Sunrise.h>
#include <lvgl.h>
#include "types.h"

// FreeRTOS handles
extern TaskHandle_t gpsTaskHandle;
extern TaskHandle_t guiTaskHandle;
extern TaskHandle_t meshtasticTaskHandle;
extern TaskHandle_t meshtasticCallbackTaskHandle;
extern TaskHandle_t eepromTaskHandle;
extern TaskHandle_t systemTaskHandle;
extern TaskHandle_t espnowTaskHandle;

// Synchronization objects
extern SemaphoreHandle_t gpsMutex;
extern SemaphoreHandle_t eepromMutex;
extern SemaphoreHandle_t displayMutex;
extern QueueHandle_t eepromWriteQueue;
extern QueueHandle_t meshtasticCallbackQueue;
extern QueueHandle_t espnowRecvQueue;

// Display objects
extern SPIClass touchscreenSpi;
extern XPT2046_Touchscreen touchscreen;
extern uint16_t touchScreenMinimumX, touchScreenMaximumX, touchScreenMinimumY, touchScreenMaximumY;
extern lv_indev_t *indev;
extern uint8_t *draw_buf;
extern uint32_t lastTick;

// GPS objects
extern HardwareSerial &gpsSerial;
extern NMEAGPS gps;
extern gps_fix fix;
extern movingAvg avgAzimuthDeg;
extern movingAvg avgSpeed;

// Time objects
extern Timezone myTZ;
extern JC_Sunrise sun;
extern time_t localTime, utcTime;
extern TimeChangeRule *tcr;

// Preferences
extern Preferences prefs;

// GPS and time variables (NOT in get_set_vars.h)
extern int localYear, localMonth, localDay, old_localDay;
extern int localHour, localMinute, localSecond, localDayOfWeek;
extern String latitude, longitude, altitude;
extern float hdop, old_max_hdop;
extern int old_day_backlight, old_night_backlight;
extern unsigned long previousGPSms;
extern float accumDistance;
extern float avg_speed_calc;  // Float version for GPS calculations

// Old tracking variables (NOT in get_set_vars.h)
extern String old_espnow_mac_addr;

// Now playing variables (NOT in get_set_vars.h)
extern String live_venue_event_data;

// Meshtastic variables (NOT in get_set_vars.h)
extern uint32_t next_send_time;
extern bool not_yet_connected;
extern bool old_mesh_serial_enabled;

// Inactivity timeout variables (NOT in get_set_vars.h)
extern uint32_t lastTouchActivity;

// ESP-NOW variables (NOT in get_set_vars.h)
extern bool espnow_enabled;
extern bool old_espnow_enabled;
extern int espnow_peer_count;

// All EEZ Studio variables are defined in get_set_vars.h
// Include it here so all files can access them
#include "get_set_vars.h"

#endif // GLOBALS_H