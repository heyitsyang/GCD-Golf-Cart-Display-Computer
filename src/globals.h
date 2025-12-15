#ifndef GLOBALS_H
#define GLOBALS_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <XPT2046_Touchscreen.h>
#include <NMEAGPS.h>
#include <Timezone.h>
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
extern SemaphoreHandle_t hotPacketMutex;  // Protects hot packet buffer swapping (not data reads)
extern QueueHandle_t eepromWriteQueue;
extern QueueHandle_t meshtasticCallbackQueue;
extern QueueHandle_t espnowRecvQueue;
extern QueueHandle_t gpsConfigCallbackQueue;

// Double buffering for hot packet data (eliminates blocking reads)
// Parser writes to back buffer, swaps atomically, GUI reads from front buffer
// Front buffer = hotPacketBuffer_xxx[hotPacketActiveBuffer] (current data for GUI reads)
// Back buffer = hotPacketBuffer_xxx[1 - hotPacketActiveBuffer] (next data being written by parser)
// Only the buffer pointer swap is protected by mutex (~10ms), not the data reads/writes
extern volatile int hotPacketActiveBuffer;  // 0 or 1, atomic swap under mutex
extern String hotPacketBuffer_wx_rcv_time[2];
extern String hotPacketBuffer_cur_temp[2];
extern String hotPacketBuffer_fcast_hr1[2];
extern String hotPacketBuffer_fcast_glyph1[2];
extern String hotPacketBuffer_fcast_temp1[2];
extern String hotPacketBuffer_fcast_precip1[2];
extern String hotPacketBuffer_fcast_hr2[2];
extern String hotPacketBuffer_fcast_glyph2[2];
extern String hotPacketBuffer_fcast_temp2[2];
extern String hotPacketBuffer_fcast_precip2[2];
extern String hotPacketBuffer_fcast_hr3[2];
extern String hotPacketBuffer_fcast_glyph3[2];
extern String hotPacketBuffer_fcast_temp3[2];
extern String hotPacketBuffer_fcast_precip3[2];
extern String hotPacketBuffer_fcast_hr4[2];
extern String hotPacketBuffer_fcast_glyph4[2];
extern String hotPacketBuffer_fcast_temp4[2];
extern String hotPacketBuffer_fcast_precip4[2];
extern String hotPacketBuffer_np_rcv_time[2];
extern String hotPacketBuffer_live_venue_event_data[2];

// Display objects
extern SPIClass touchscreenSpi;
extern XPT2046_Touchscreen touchscreen;
extern uint16_t touchScreenMinimumX, touchScreenMaximumX, touchScreenMinimumY, touchScreenMaximumY;
extern lv_indev_t *indev;
extern uint8_t *draw_buf;
extern uint32_t lastTick;

// Touchscreen calibration coefficients (loaded from EEPROM if available)
// If not available, falls back to existing auto-calibration using map() function
extern float touch_alpha_x, touch_beta_x, touch_delta_x;
extern float touch_alpha_y, touch_beta_y, touch_delta_y;
extern bool use_touch_calibration;  // true if calibration coefficients loaded from EEPROM

// GPS objects
extern HardwareSerial &gpsSerial;
extern NMEAGPS gps;
extern gps_fix fix;

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
extern float hdop;
extern int old_day_backlight, old_night_backlight;
extern unsigned long lastGpsTimeUpdate;  // Tracks when GPS time was last received
extern float avg_speed_calc;  // Float version for GPS calculations

// Home location variables (NOT in get_set_vars.h)
extern float homeLatitude;    // Home location latitude
extern float homeLongitude;   // Home location longitude
extern bool homeLocationSet;  // True if home location has been saved

// Old tracking variables (NOT in get_set_vars.h)
extern String old_espnow_gci_mac_addr;
extern bool old_flip_screen;
extern float old_temperature_adj;

// Now playing variables (NOT in get_set_vars.h)
extern String live_venue_event_data;

// Meshtastic variables (NOT in get_set_vars.h)
extern uint32_t next_send_time;
extern bool not_yet_connected;
extern bool old_mesh_serial_enabled;
extern bool wakeNotificationSent;
extern bool gpsConfigAttempted;

// Inactivity timeout variables (NOT in get_set_vars.h)
extern uint32_t lastTouchActivity;

// ESP-NOW variables (NOT in get_set_vars.h)
extern bool espnow_enabled;
extern bool old_espnow_enabled;
extern int espnow_peer_count;

// Golf cart interface variables for incoming data
extern int modeHeadLights;
extern int outdoorLuminosity;
extern float airTemperature;
extern float battVoltage;
extern float fuelLevel;
extern structMsgFromGci dataFromGci;

// Golf cart interface variables for outbound data
extern int cmdToGci;
extern structMsgToGci dataToGci;

// Speaker
extern int32_t old_speaker_volume;

// Service interval (manually adjustable, needs old value tracking)
extern int32_t old_svc_interval_hrs;

// All EEZ Studio variables are defined in get_set_vars.h
// Include it here so all files can access them
#include "get_set_vars.h"

#endif // GLOBALS_H