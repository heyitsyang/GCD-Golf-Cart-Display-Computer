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
QueueHandle_t eepromWriteQueue;
QueueHandle_t meshtasticCallbackQueue;
QueueHandle_t espnowRecvQueue;

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

// GPS objects
HardwareSerial &gpsSerial = Serial;
NMEAGPS gps;
gps_fix fix;
movingAvg avgAzimuthDeg(8);
movingAvg avgSpeed(10);

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
float hdop, old_max_hdop;
int old_day_backlight, old_night_backlight;
unsigned long previousGPSms = 0;
float accumDistance;
float avg_speed_calc = 0.0;  // Float version for GPS calculations

// Old tracking variables
String old_espnow_mac_addr;
bool old_flip_screen = false;
int old_speaker_volume = 10;

// Now playing variables
String live_venue_event_data = "";

// Meshtastic variables
uint32_t next_send_time = 0;
bool not_yet_connected = true;
bool old_mesh_serial_enabled = true;

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