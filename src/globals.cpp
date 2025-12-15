#include "globals.h"
#include "config.h"

// FreeRTOS handles
TaskHandle_t gpsTaskHandle = NULL;
TaskHandle_t guiTaskHandle = NULL;
TaskHandle_t meshtasticTaskHandle = NULL;
TaskHandle_t meshtasticCallbackTaskHandle = NULL;
TaskHandle_t eepromTaskHandle = NULL;
TaskHandle_t systemTaskHandle = NULL;
TaskHandle_t espnowTaskHandle = NULL;

// Synchronization objects
SemaphoreHandle_t gpsMutex;
SemaphoreHandle_t eepromMutex;
SemaphoreHandle_t displayMutex;
SemaphoreHandle_t hotPacketMutex;  // Protects hot packet buffer swapping (not data reads)
QueueHandle_t eepromWriteQueue;
QueueHandle_t meshtasticCallbackQueue;
QueueHandle_t espnowRecvQueue;
QueueHandle_t gpsConfigCallbackQueue;

// Double buffering for hot packet data (eliminates blocking reads)
volatile int hotPacketActiveBuffer = 0;  // 0 or 1
String hotPacketBuffer_wx_rcv_time[2];
String hotPacketBuffer_cur_temp[2];
String hotPacketBuffer_fcast_hr1[2];
String hotPacketBuffer_fcast_glyph1[2];
String hotPacketBuffer_fcast_temp1[2];
String hotPacketBuffer_fcast_precip1[2];
String hotPacketBuffer_fcast_hr2[2];
String hotPacketBuffer_fcast_glyph2[2];
String hotPacketBuffer_fcast_temp2[2];
String hotPacketBuffer_fcast_precip2[2];
String hotPacketBuffer_fcast_hr3[2];
String hotPacketBuffer_fcast_glyph3[2];
String hotPacketBuffer_fcast_temp3[2];
String hotPacketBuffer_fcast_precip3[2];
String hotPacketBuffer_fcast_hr4[2];
String hotPacketBuffer_fcast_glyph4[2];
String hotPacketBuffer_fcast_temp4[2];
String hotPacketBuffer_fcast_precip4[2];
String hotPacketBuffer_np_rcv_time[2];
String hotPacketBuffer_live_venue_event_data[2];

// Display objects
SPIClass touchscreenSpi = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
uint16_t touchScreenMinimumX = 200;
uint16_t touchScreenMaximumX = 3700;
uint16_t touchScreenMinimumY = 240;
uint16_t touchScreenMaximumY = 3800;
lv_indev_t *indev;
uint8_t *draw_buf;
uint32_t lastTick = 0;

// Touchscreen calibration coefficients (default to 0, loaded from EEPROM if available)
float touch_alpha_x = 0.0;
float touch_beta_x = 0.0;
float touch_delta_x = 0.0;
float touch_alpha_y = 0.0;
float touch_beta_y = 0.0;
float touch_delta_y = 0.0;
bool use_touch_calibration = false;  // Set to true when coefficients loaded from EEPROM

// GPS objects
HardwareSerial &gpsSerial = Serial;
NMEAGPS gps;
gps_fix fix;

// Time zone definitions
TimeChangeRule mySTD = {"EST", First, Sun, Nov, 2, -300};
TimeChangeRule myDST = {"EDT", Second, Sun, Mar, 2, -240};
TimeChangeRule *tcr;
Timezone myTZ(myDST, mySTD);
JC_Sunrise sun{MY_LATITUDE, MY_LONGITUDE, JC_Sunrise::officialZenith};

// Time variables
time_t localTime, utcTime;
int timesetinterval = 60;

// Preferences
Preferences prefs;

// GPS and time variables (NOT in get_set_vars.h)
int localYear, localMonth, localDay = 0, old_localDay = 0;
int localHour, localMinute, localSecond, localDayOfWeek;
String latitude;
String longitude;
String altitude;
float hdop;
int old_day_backlight, old_night_backlight;
unsigned long lastGpsTimeUpdate = 0;  // Tracks when GPS time was last received
float avg_speed_calc = 0.0;  // Float version for GPS calculations

// Home location variables
float homeLatitude = 0.0;
float homeLongitude = 0.0;
bool homeLocationSet = false;

// Old tracking variables
String old_espnow_gci_mac_addr;
bool old_flip_screen = false;
int32_t old_speaker_volume = 10;
int32_t old_svc_interval_hrs = 100;
float old_temperature_adj = 0.0;

// Now playing variables
String live_venue_event_data = "";

// Meshtastic variables
uint32_t next_send_time = 0;
bool not_yet_connected = true;
bool old_mesh_serial_enabled = true;
bool wakeNotificationSent = false;
bool gpsConfigAttempted = false;

// Inactivity timeout variables
uint32_t lastTouchActivity = 0;

// ESP-NOW variables
bool espnow_enabled = true;
bool old_espnow_enabled = false;
int espnow_peer_count = 0;

// Golf cart interface variables for incoming data
int modeHeadLights = -99;
int outdoorLuminosity = -99;
float airTemperature = -99;
float battVoltage = -99;
float fuelLevel = -99;
structMsgFromGci dataFromGci;

// Golf cart interface variables for outbound data
int cmdToGci;
structMsgToGci dataToGci;

// Note: All EEZ Studio shared variables (cur_date, version, etc.)
// are defined in get_set_vars.cpp via get_set_vars.h