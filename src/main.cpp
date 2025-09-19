/********************************************************************************************\
*    CYD-EEZ-LVGL Template - RTOS Version                                                    *
*                                                                                            *
*    Refactored to use FreeRTOS tasks for better concurrent processing                       *
*    Maintains original UART0 split configuration (RX=GPS, TX=Debug)                        *
*    Dedicated Meshtastic callback task to prevent stack overflow                            *
*    CONVERTED TO USE NEOGPS LIBRARY FOR GNSS COMPATIBILITY                                  *
*                                                                                            *
\********************************************************************************************/

/********************************************
 *                                          *
 * THIS FILE IS FOR THE CYD 2432S028R BOARD *
 * - Resistive touch                        *
 * - ILI9341 screen                         *
 *                                          *
 ********************************************/

/********************
 *     INCLUDES     *
 ********************/

#include <lvgl.h>                //  lvgl@^9.3.0
#include <TFT_eSPI.h>            //  bodmer/TFT_eSPI@^2.5.43
#include <XPT2046_Touchscreen.h> //  https://github.com/PaulStoffregen/XPT2046_Touchscreen.git
#include "NMEAGPS_cfg.h"         //  Custom NeoGPS configuration
#include <NMEAGPS.h>             //  NeoGPS library - supports GNSS sentences
#include <Timezone.h>            // https://github.com/JChristensen/Timezone
#include <movingAvg.h>
#include <Preferences.h> // used to save preferences in EEPROM
#include <JC_Sunrise.h>  // https://github.com/JChristensen/JC_Sunrise
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "version.h"
#include "ui/ui.h"        // generated from EEZ Studio
#include "ui/actions.h"
#include "get_set_vars.h" // get & set functions for EEZ Studio vars
#include "prototypes.h"   // declare functions so they can be moved below setup() & loop()
#include "Meshtastic.h"


/********************
 *      DEFINES     *
 ********************/

// enabling any DEBUG bogs down the loop
#define DEBUG_TOUCH_SCREEN 0
#define DEBUG_GPS 0
// #define DEBUG_MESHTASTIC 1
#define MT_DEBUGGING 0 // set to 1 for in-depth debugging

// Touch Screen pins
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// eSPI touchscreen setRotation() values
#define TOUCH_ROTATION_0 0
#define TOUCH_ROTATION_90 1
#define TOUCH_ROTATION_180 2
#define TOUCH_ROTATION_270 3

#define TOUCH_WIDTH 320 // define touch using LANDSCAPE orientation
#define TOUCH_HEIGHT 240

#define TFT_WIDTH 240  // define this display/driver using PORTRAIT orientation
#define TFT_HEIGHT 320 // (XY origin can differ between displays/drivers)

#define TFT_BACKLIGHT_PIN 21
#define LEDC_CHANNEL_0 0     // use first channel of 16 channels (started from zero)
#define LEDC_BASE_FREQ 5000  // use 5000 Hz as a LEDC base frequency
#define LEDC_TIMER_12_BIT 12 // use 12 bit precision for LEDC timer
#define MAX_BACKLIGHT_VALUE 255

/*LVGL draw into this buffer, 1/10 screen size usually works well. The size is in bytes*/
#define NUM_BUFS 2
#define DRAW_BUF_SIZE (TFT_WIDTH * TFT_HEIGHT * NUM_BUFS / 10 * (LV_COLOR_DEPTH / 8))

#define MT_SERIAL_TX_PIN 22   // TX to Meshtastic
#define MT_SERIAL_RX_PIN 27   // RX from Meshtastic
#define MT_DEV_BAUD_RATE 9600 // Meshtastic device baud rate

#define GPS_RX_PIN 03           // USB connector RX UART0
#define GPS_TX_PIN 01           // USB connector TX UART0
#define GPS_BAUD 9600           // gpsSerial & Serial baud rate
#define GPS_READ_INTERVAL 15000 // in ms

// Needed for JCSunrise library before GPS has signal
#define MY_LATITUDE 28.8522f
#define MY_LONGITUDE -82.0028f

// Send a text message every this many seconds
#define SEND_PERIOD 300

#define MAX_MESHTASTIC_PAYLOAD 237 // max actual payload bytes after subtracting overhead
#define HOT_PKT_HEADER_OFFSET 5    // used in decoding HoT packets which have a header

// Task Stack Sizes (in bytes)
#define GPS_TASK_STACK_SIZE 4096
#define GUI_TASK_STACK_SIZE 8192
#define MESHTASTIC_TASK_STACK_SIZE 4096
#define MESHTASTIC_CALLBACK_TASK_STACK_SIZE 6144 // Dedicated task for callbacks
#define EEPROM_TASK_STACK_SIZE 2048
#define SYSTEM_TASK_STACK_SIZE 2048

// Task Priorities (higher number = higher priority)
#define GUI_TASK_PRIORITY 3
#define GPS_TASK_PRIORITY 2
#define MESHTASTIC_TASK_PRIORITY 2
#define MESHTASTIC_CALLBACK_TASK_PRIORITY 2
#define EEPROM_TASK_PRIORITY 1
#define SYSTEM_TASK_PRIORITY 1

/******************************
 *    TYPE DEFINITIONS        *
 ******************************/

/* EEPROM Write Queue Item */
typedef enum
{
  EEPROM_FLOAT,
  EEPROM_INT,
  EEPROM_STRING
} eepromType_t;

typedef struct
{
  eepromType_t type;
  char key[32];
  union
  {
    float floatVal;
    int intVal;
    char stringVal[64];
  } value;
} eepromWriteItem_t;

/* Meshtastic Message Queue Item */
typedef struct
{
  uint32_t dest;
  uint8_t channel;
  char message[MAX_MESHTASTIC_PAYLOAD];
  uint32_t timestamp;
  uint8_t retryCount;
  bool isPending;
} meshtasticMessageItem_t;
typedef struct
{
  uint32_t from;
  uint32_t to;
  uint8_t channel;
  char text[MAX_MESHTASTIC_PAYLOAD];
} meshtasticCallbackItem_t;

/******************************
 *    VARIABLES & OBJECTS     *
 ******************************/

/* Display variables */
SPIClass touchscreenSpi = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
uint16_t touchScreenMinimumX = 200, touchScreenMaximumX = 3700, touchScreenMinimumY = 240, touchScreenMaximumY = 3800;
lv_indev_t *indev;     // Touchscreen input device
uint8_t *draw_buf;     // draw_buf is allocated on heap otherwise the static area is too big on ESP32 at compile
uint32_t lastTick = 0; // Used to track the tick timer

/* FreeRTOS handles */
TaskHandle_t gpsTaskHandle = NULL;
TaskHandle_t guiTaskHandle = NULL;
TaskHandle_t meshtasticTaskHandle = NULL;
TaskHandle_t meshtasticCallbackTaskHandle = NULL;
TaskHandle_t eepromTaskHandle = NULL;
TaskHandle_t systemTaskHandle = NULL;

/* Synchronization objects */
SemaphoreHandle_t gpsMutex;
SemaphoreHandle_t eepromMutex;
SemaphoreHandle_t displayMutex;
QueueHandle_t eepromWriteQueue;
QueueHandle_t meshtasticCallbackQueue;

/* App variables */

// GPS and debug serial use the same serial port - GPS uses RX, normal serial debug Serial.print uses TX
// To clarify, gpsSerial is used for gps functions - Serial is used for debug serial printing
HardwareSerial &gpsSerial = Serial; // create gpsSerial alias to Serial so we can use gpsSerial even though it is the same serial port

// Define movingAvg objects
movingAvg avgAzimuthDeg(8), avgSpeed(10); // value is number of samples

// NeoGPS objects - using UART0 for both GPS RX and debug TX
NMEAGPS gps;      // GPS parser
gps_fix fix;      // GPS fix data

// GPS Time
int localYear;

// Preferences
Preferences prefs;

// GPS and time variables (protected by gpsMutex)
int localMonth, localDay = 0, old_localDay = 0, localHour, localMinute, localSecond, localDayOfWeek;
String latitude;
String longitude;
String altitude;
float hdop, old_max_hdop;
int old_day_backlight, old_night_backlight;
unsigned long previousGPSms = 0;
float accumDistance;

// time zone & sunrise/sunset definitions
TimeChangeRule mySTD = {"EST", First, Sun, Nov, 2, -300};  // UTC - 5 hours
TimeChangeRule myDST = {"EDT", Second, Sun, Mar, 2, -240}; // UTC - 4 hours
TimeChangeRule *tcr;                                       // pointer to use to extract TZ abbreviation later
Timezone myTZ(myDST, mySTD);

JC_Sunrise sun{MY_LATITUDE, MY_LONGITUDE, JC_Sunrise::officialZenith}; // set lat & lon for sunrise/sunset calculation

// time variables
time_t localTime, utcTime;
int timesetinterval = 60; // set microcontroller time every 60 seconds

// the rest are defined in get_set_vars.h as part of EEZ Studio integration
// String cur_date;
// String hhmm_str;
// String hhmmss_str;
// String strAmPm;
// String heading;
// String sats_hdop;
// String satellites;
// int avg_speed;
// float max_hdop;

// Meshtastic
uint32_t next_send_time = 0;
bool not_yet_connected = true;
bool old_mesh_comm = true;

// ESPNow
String old_espnow_mac_addr;

// Live venue/event data from Meshtastic
String live_venue_event_data = ""; // Stores the most recent HoTPktType 2 data

/***********************
 *    TASK FUNCTIONS   *
 ***********************/

// GPS Task - Handles all GPS processing using NeoGPS with UART0 split design
void gpsTask(void *parameter)
{
  time_t sunrise_t, sunset_t;

  while (true)
  {
    unsigned long currentGPSms = millis();
    int elapsed_gps_read;
    float incDistance;

    elapsed_gps_read = currentGPSms - previousGPSms;
    if (elapsed_gps_read >= GPS_READ_INTERVAL)
    {
      // Take GPS mutex to update shared variables
      if (xSemaphoreTake(gpsMutex, portMAX_DELAY))
      {
        previousGPSms = currentGPSms;     // Update the last execution time
        
        // Process all available GPS data - we want the most recent time data
        // Location data every 15 seconds is fine, but time needs to be current
        gps_fix latestValidFix;
        bool foundValidLocation = false;
        bool foundValidTime = false;
        
        // Process all accumulated GPS data to get the most recent valid information
        while (gpsSerial.available() > 0)
        {
          char c = gpsSerial.read();
          if (gps.decode(c) == NMEAGPS::DECODE_COMPLETED)
          {
            // Get the completed fix
            fix = gps.fix();
            
            // Always capture the most recent valid time data
            if (fix.valid.date && fix.valid.time)
            {
              latestValidFix = fix;
              foundValidTime = true;
            }
            
            // Use the most recent valid location data
            if (fix.valid.location)
            {
              latestValidFix = fix;  // This will overwrite with most recent location AND time
              foundValidLocation = true;
            }
          }
        }
        
        // After processing all data, use the most recent valid information
        if (foundValidTime)
        {
          // Always update time from the most recent valid time data
          setTime(latestValidFix.dateTime.hours, latestValidFix.dateTime.minutes, latestValidFix.dateTime.seconds, 
                 latestValidFix.dateTime.date, latestValidFix.dateTime.month, latestValidFix.dateTime.full_year());
                 
          utcTime = now();
          localTime = myTZ.toLocal(utcTime, &tcr);
          localYear = year(localTime);
          localMonth = month(localTime);
          localDay = day(localTime);
          localHour = hour(localTime);
          localMinute = minute(localTime);
          localSecond = second(localTime);
          localDayOfWeek = weekday(localTime);

          // Update time display strings
          cur_date = String(getDayAbbr(localDayOfWeek)) + ", " + String(getMonthAbbr(localMonth)) + " " + String(localDay);
          hhmm_str = String(make12hr(localHour)) + ":" + String(prefix_zero(localMinute));
          hhmmss_str = String(make12hr(localHour)) + ":" + String(prefix_zero(localMinute)) + String(prefix_zero(localSecond));
          am_pm_str = am_pm(localHour);
        }
        
        // Only update location-dependent data if we have recent location data
        if (foundValidLocation)
        {
          // Set variables related to GPS location
          latitude = String(latestValidFix.latitude(), 6);
          longitude = String(latestValidFix.longitude(), 6);

          // Handle altitude if available
          if (latestValidFix.valid.altitude)
          {
            altitude = String(latestValidFix.altitude(), 2);
          }

          // Handle HDOP - with your NMEAGPS_cfg.h configuration, this should work
          #ifdef NMEAGPS_PARSE_HDOP
            if (latestValidFix.valid.hdop)
            {
              hdop = latestValidFix.hdop / 1000.0; // NeoGPS provides HDOP in thousandths
            }
            else
            {
              // Fallback to satellite count estimation
              if (latestValidFix.valid.satellites && latestValidFix.satellites > 0)
              {
                if (latestValidFix.satellites >= 6) {
                  hdop = 1.5; // Good fix
                } else if (latestValidFix.satellites >= 4) {
                  hdop = 2.0; // Acceptable fix
                } else {
                  hdop = 5.0; // Poor fix
                }
              }
            }
          #else
            // HDOP parsing not enabled, use satellite count estimation
            if (latestValidFix.valid.satellites && latestValidFix.satellites > 0)
            {
              if (latestValidFix.satellites >= 6) {
                hdop = 1.5; // Good fix
              } else if (latestValidFix.satellites >= 4) {
                hdop = 2.0; // Acceptable fix
              } else {
                hdop = 5.0; // Poor fix
              }
            }
          #endif

          // Update satellites/HDOP display string
          if (latestValidFix.valid.satellites)
          {
            sats_hdop = String(latestValidFix.satellites) + "/" + String(hdop, 2);
          }

          // Only update speed and heading if HDOP indicates reliable data
          if (hdop < max_hdop)
          { 
            // Handle speed
            if (latestValidFix.valid.speed)
            {
              // NeoGPS speed conversion to mph
              float speedMph = latestValidFix.speed_mph();
              avg_speed = avgSpeed.reading(speedMph);
            }
            
            // Handle heading/course
            if (latestValidFix.valid.heading)
            {
              // NeoGPS heading is in centidegrees, convert to degrees
              float headingDeg = latestValidFix.heading_cd() / 100.0;
              float avgHeading = avgAzimuthDeg.reading(headingDeg);
              
              // Convert heading to cardinal direction (replaces TinyGPS++ cardinal function)
              const char* directions[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", 
                                        "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
              int dirIndex = (int)((avgHeading + 11.25) / 22.5) % 16;
              heading = String(directions[dirIndex]);
            }
          }

          /*
           * Set the display's backlight based on sunrise/sunset
           */
          if (localDay != old_localDay) // get new sunrise & sunset time
            sun.calculate(localTime, tcr->offset, sunrise_t, sunset_t);

          // stored backlight values from the GUI are 1-10, actual scale is 1-255
          if ((localTime > sunrise_t) && (localTime < sunset_t))
          {
            ledcAnalogWrite(LEDC_CHANNEL_0, ((day_backlight * 20) + 55), MAX_BACKLIGHT_VALUE);
            // Serial.print("now using day_backlight value: ");
            // Serial.println(day_backlight);
          }
          else
          {
            ledcAnalogWrite(LEDC_CHANNEL_0, (night_backlight * 20), MAX_BACKLIGHT_VALUE);
            // Serial.print("now using night_backlight value: ");
            // Serial.println(night_backlight);
          }

          /*
           * Approximate distance traveled
           */
          incDistance = avg_speed * elapsed_gps_read / 3600000;
          accumDistance = accumDistance + incDistance;

#if DEBUG_GPS == 1 // comment out lines not necessary for your debug
          // vars not dependent on GPS signal
          Serial.print("\nutcTime: ");
          Serial.print(utcTime);
          Serial.print(ctime(&utcTime));
          Serial.print("localTime: ");
          Serial.print(localTime);
          Serial.print(ctime(&localTime));
          Serial.print("\nsunrise: ");
          Serial.print(sunrise_t);
          Serial.print(ctime(&sunrise_t));
          Serial.print("sunset: ");
          Serial.print(sunset_t);
          Serial.print(ctime(&sunset_t));
          Serial.print("Max HDOP = ");
          Serial.println(max_hdop);

          // vars dependent on GPS signal
          Serial.println("");
          Serial.print("elapsed_gps_read: ");
          Serial.println(elapsed_gps_read);
          Serial.print("incDistance: ");
          Serial.println(incDistance);
          Serial.print("accumDistance: ");
          Serial.println(accumDistance);
          Serial.print("LAT: ");
          Serial.println(latitude);
          Serial.print("LONG: ");
          Serial.println(longitude);
          Serial.print("AVG_SPEED (mph) = ");
          Serial.println(avg_speed);
          Serial.print("DIRECTION = ");
          Serial.println(heading);
          Serial.print("ALT (m)= ");
          Serial.println(altitude);
          Serial.print("Sats/HDOP = ");
          Serial.println(sats_hdop);
          Serial.print("Local time: ");
          Serial.println(String(localYear) + "-" + String(localMonth) + "-" + String(localDay) + ", " + String(localHour) + ":" + String(localMinute) + ":" + String(localSecond));
          Serial.println(cur_date + " " + hhmm_str);
#endif
        }

        xSemaphoreGive(gpsMutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // Check GPS every 100ms
  }
}

// GUI Task - Handles LVGL and touchscreen
void guiTask(void *parameter)
{
  while (true)
  {
    /* Required for LVGL */
    lv_tick_inc(millis() - lastTick); // Update the tick timer. Tick is new for LVGL 9.x
    lastTick = millis();
    lv_timer_handler(); // Update the UI for LVGL
    ui_tick();          // Update the UI for EEZ Studio Flow

    vTaskDelay(pdMS_TO_TICKS(5)); // not sure why this is necessary - keeping original timing
  }
}

// Meshtastic Task - High frequency to ensure mt_loop() is called continuously
void meshtasticTask(void *parameter)
{
  while (true)
  {
    // Handle mesh communication enable/disable
    if (mesh_comm != old_mesh_comm)
    {
      old_mesh_comm = mesh_comm;
      if (mesh_comm == false)
      {
        mt_serial_end(); // stop the port
        Serial.println("meshSerial disabled");
      }
      else
      {
        mt_serial_init(MT_SERIAL_RX_PIN, MT_SERIAL_TX_PIN, MT_DEV_BAUD_RATE); // start meshtastic serial port
        Serial.println("meshSerial enabled");
      }
    }

    // Record the time that this loop began (in milliseconds since the device booted)
    uint32_t now = millis();

    // Run the Meshtastic loop - THIS MUST BE CALLED CONTINUOUSLY
    bool can_send = mt_loop(now);

    // If we can send, and it's time to do so, send a text message and schedule the next one.
    if (can_send && now >= next_send_time)
    {
      Serial.print("\nSending test message at: ");
      Serial.println(hhmm_str);

      // Change this to a specific node number if you want to send to just one node
      uint32_t dest = BROADCAST_ADDR;
      // Change this to another input if you want to send on a different channel
      // Remember routine Meshtastic telemetry is only sent on channel 0
      uint8_t channel_index = 0; // For the golf cart:  0 = Telemetry, 1 = TheVillages, 2 = LongFast

      bool success = mt_send_text("Hello, world from the CYD Golf Cart Computer!", dest, channel_index);
      Serial.print("mt_send_text returned: ");
      Serial.println(success ? "SUCCESS" : "***** FAILED ****");

      next_send_time = now + SEND_PERIOD * 1000;
      Serial.printf("Next send time in: %d minutes\n", (SEND_PERIOD / 60));

    }

    // Minimal delay to prevent watchdog issues but keep mt_loop() frequency high
    vTaskDelay(pdMS_TO_TICKS(1)); // 1ms delay = 1000Hz mt_loop() calls
  }
}

// Meshtastic Callback Task - Handles message processing with adequate stack space
void meshtasticCallbackTask(void *parameter)
{
  meshtasticCallbackItem_t item;

  while (true)
  {
    // Wait for callback messages
    if (xQueueReceive(meshtasticCallbackQueue, &item, portMAX_DELAY))
    {

      char scratch[MAX_MESHTASTIC_PAYLOAD] = {0}; // use as scratchpad buffer

      // Do your own thing here. This example just prints the message to the serial console.
      Serial.print("Received a text message on channel: ");
      Serial.print(item.channel);
      Serial.print(" from: ");
      Serial.print(item.from);
      Serial.print(" to: ");
      Serial.print(item.to);
      Serial.print(" message: ");
      Serial.println(item.text);
      if (item.to == 0xFFFFFFFF)
      {
        Serial.println("This is a BROADCAST message.");
      }
      else if (item.to == my_node_num)
      {
        Serial.println("This is a DM to me!");
      }
      else
      {
        Serial.println("This is a DM to someone else.");
      }

      // coded telemetry message
      // example wx packet:   |#01#76#1pm,4,0.0#2pm,4,0.0#3pm,7,0.03#4pm,7,0.01#
      // where | is start of pkt character for a HoT pkt, 01 is telemetry pkt type (01 = wx), and the rest is wx pkt payload
      if (item.text[0] == '|')
      {
        Serial.println("Received a HoT pkt");
        // Now, combine the 3rd and 4th characters into a single integer.
        // Memory-efficient method: Perform the conversion manually.
        // '0' is 48 in ASCII, '1' is 49. '1' - '0' = 1.
        int HotPktType = (item.text[2] - '0') * 10 + (item.text[3] - '0');

        // Handle potential invalid input for robustness
        if (item.text[2] < '0' || item.text[2] > '9' || item.text[3] < '0' || item.text[3] > '9')
        {
          Serial.print("Malformed HotPktType:  ");
          Serial.println(HotPktType);
        }
        else
        {

          // Process the extracted number using a switch statement
          switch (HotPktType)
          {
          case 1: // wx packet
            Serial.println("WX packet received");
            wx_rcv_time = cur_date + "  " + hhmm_str + am_pm_str;
            parseWeatherData((char *)item.text);
            break;
          case 2: // venue/event data packet
            Serial.println("Venue/Event packet received");  // Format expected: |#02#venue1,event1#venue2,event2#venue3,event3#...
            np_rcv_time = cur_date + "  " + hhmm_str + am_pm_str;
            live_venue_event_data = String(&item.text[HOT_PKT_HEADER_OFFSET]);  // Store the raw data starting after the HoT packet header
            Serial.print("Stored venue/event data: ");
            Serial.println(live_venue_event_data);
            break;
          // Add more cases as needed
          default:
            Serial.print("Unrecognized HotPktType:  ");
            Serial.println(HotPktType);
            break;
          }
        }
      }
    }
  }
}

// EEPROM Task - Handles all EEPROM write operations
void eepromTask(void *parameter)
{
  eepromWriteItem_t item;

  while (true)
  {
    // Wait for EEPROM write requests
    if (xQueueReceive(eepromWriteQueue, &item, portMAX_DELAY))
    {
      if (xSemaphoreTake(eepromMutex, portMAX_DELAY))
      {
        switch (item.type)
        {
        case EEPROM_FLOAT:
          prefs.putFloat(item.key, item.value.floatVal);
          Serial.print("> ");
          Serial.print(item.key);
          Serial.print(" saved to eeprom:");
          Serial.println(item.value.floatVal);
          break;
        case EEPROM_INT:
          prefs.putInt(item.key, item.value.intVal);
          Serial.print("< ");
          Serial.print(item.key);
          Serial.print(" saved to eeprom:");
          Serial.println(item.value.intVal);
          break;
        case EEPROM_STRING:
          prefs.putString(item.key, String(item.value.stringVal));
          Serial.print("> ");
          Serial.print(item.key);
          Serial.print(" saved to eeprom:");
          Serial.println(item.value.stringVal);
          break;
        }
        xSemaphoreGive(eepromMutex);
      }
    }
  }
}

// System Task - Handles system monitoring and EEPROM change detection
void systemTask(void *parameter)
{
  while (true)
  {
    // Handle manual reboot
    if (manual_reboot == true)
    { // rebooted from UI
      ESP.restart();
    }

    /*
     * Write values to eeprom only if changed
     */
    if (day_backlight != old_day_backlight)
    {
      ledcAnalogWrite(LEDC_CHANNEL_0, ((day_backlight * 20) + 55), MAX_BACKLIGHT_VALUE);

      eepromWriteItem_t item;
      item.type = EEPROM_INT;
      strcpy(item.key, "day_backlight");
      item.value.intVal = day_backlight;
      xQueueSend(eepromWriteQueue, &item, 0);

      old_day_backlight = day_backlight;
    }

    if (night_backlight != old_night_backlight)
    {
      ledcAnalogWrite(LEDC_CHANNEL_0, (night_backlight * 20), MAX_BACKLIGHT_VALUE);

      eepromWriteItem_t item;
      item.type = EEPROM_INT;
      strcpy(item.key, "night_backlight");
      item.value.intVal = night_backlight;
      xQueueSend(eepromWriteQueue, &item, 0);

      old_night_backlight = night_backlight;
    }

    if (max_hdop != old_max_hdop)
    {
      eepromWriteItem_t item;
      item.type = EEPROM_FLOAT;
      strcpy(item.key, "max_hdop");
      item.value.floatVal = max_hdop;
      xQueueSend(eepromWriteQueue, &item, 0);

      old_max_hdop = max_hdop;
    }

    if (espnow_mac_addr != old_espnow_mac_addr)
    {
      eepromWriteItem_t item;
      item.type = EEPROM_STRING;
      strcpy(item.key, "espnow_mac_addr");
      strcpy(item.value.stringVal, espnow_mac_addr.c_str());
      xQueueSend(eepromWriteQueue, &item, 0);

      old_espnow_mac_addr = espnow_mac_addr;
    }

    vTaskDelay(pdMS_TO_TICKS(1000)); // Check every second
  }
}

/*****************
 *     SETUP     *
 *****************/

void setup()
{

  /*
   * UART0 TX/RX lines are used in this manner:
   *   - RX is wired to GPS using serial TTL and bypasses the UART0 completely
   *   - TX is used for serial print messages for debugging and not connected to the GPS
   *     but can be monitored with normal a USB connection to UART0 (USB connector)
   */

  // Initialize Serial (aka gpsSerial) with the defined RX and TX pins and a baud rate of 9600
  Serial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.print("\ngpsSerial started at 9600 baud rate");

  // Print version on the Serial console
  version = String('v') + String(VERSION);
  String SW_Version = "\nSW ";
  SW_Version += version;
  Serial.println(SW_Version);

  cyd_mac_addr = String(WiFi.macAddress());
  Serial.println(cyd_mac_addr);

  // Initialize Preferences namespace
  prefs.begin("eeprom", false);

  // Initialize EEPROM stored variables
  max_hdop = prefs.getFloat("max_hdop", 2.5); // if no such max_hdop, default is the second parameter
  Serial.print("> max_hdop read from eeprom = ");
  Serial.println(max_hdop);
  old_max_hdop = max_hdop;

  day_backlight = prefs.getInt("day_backlight", 10); // if no such day_backlight, default is the second parameter
  Serial.print("> day_backlight read from eeprom = ");
  Serial.println(day_backlight);
  old_day_backlight = day_backlight;

  night_backlight = prefs.getInt("night_backlight", 5); // if no such night_backlight, default is the second parameter
  Serial.print("> night_backlight  read from eeprom = ");
  Serial.println(night_backlight);
  old_night_backlight = night_backlight;

  espnow_mac_addr = prefs.getString("espnow_mac_addr", "NONE"); // if no such espnow_mac_addr, default is the second parameter
  Serial.print("> espnow_mac_addr read from eeprom = ");
  Serial.println(espnow_mac_addr);
  old_espnow_mac_addr = espnow_mac_addr;

  // Initialize GPS
  avgAzimuthDeg.begin();
  avgSpeed.begin();

  // Initialize the touchscreen
  touchscreenSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS); // Start second SPI bus for touchscreen
  touchscreen.begin(touchscreenSpi);                                         // Touchscreen init
  touchscreen.setRotation(TOUCH_ROTATION_180);

  // Initialize LVGL
  String LVGL_Arduino = "LVGL ";
  LVGL_Arduino += String('v') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.println(LVGL_Arduino);

  cur_date = String("NO GPS");          // display this until GPS date is available
  cur_temp = String("--");
  wx_rcv_time = String(" NO DATA YET"); // display until weather report is available
  np_rcv_time = String(" NO DATA YET"); // display until now playing report is available

  lv_init(); // initialize LVGL GUI

  draw_buf = new uint8_t[DRAW_BUF_SIZE];
  lv_display_t *disp;
  disp = lv_tft_espi_create(TFT_WIDTH, TFT_HEIGHT, draw_buf, DRAW_BUF_SIZE);
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270); // set display rotation - this is independent of display

  // Initialize the XPT2046 input device driver
  indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);

  Serial.println("LVGL Setup done");

  // Integrate GUI from EEZ
  ui_init();

// Initialize screen backlight
#if ESP_IDF_VERSION_MAJOR == 5
  ledcAttach(TFT_BACKLIGHT_PIN, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
#else
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledcAttachPin(TFT_BACKLIGHT_PIN, LEDC_CHANNEL_0);
#endif

  ledcAnalogWrite(LEDC_CHANNEL_0, 225, MAX_BACKLIGHT_VALUE); // set backlight to an initial visible state

  // Initialize Meshtastic
  mt_serial_init(MT_SERIAL_RX_PIN, MT_SERIAL_TX_PIN, MT_DEV_BAUD_RATE); // start meshtastic serial port

  randomSeed(micros()); // randomise the random() function using micros()

  mt_request_node_report(connected_callback); // Initial connection to the Meshtastic device

  set_text_message_callback(text_message_callback); // Register a callback function to be called whenever a text message is received

  manual_reboot = false;
  mesh_comm = true;

  // Create FreeRTOS synchronization objects
  gpsMutex = xSemaphoreCreateMutex();
  eepromMutex = xSemaphoreCreateMutex();
  displayMutex = xSemaphoreCreateMutex();
  eepromWriteQueue = xQueueCreate(10, sizeof(eepromWriteItem_t));
  meshtasticCallbackQueue = xQueueCreate(5, sizeof(meshtasticCallbackItem_t));

  // Create FreeRTOS tasks
  xTaskCreatePinnedToCore(
      gpsTask,             // Task function
      "GPS Task",          // Task name
      GPS_TASK_STACK_SIZE, // Stack size
      NULL,                // Parameters
      GPS_TASK_PRIORITY,   // Priority
      &gpsTaskHandle,      // Task handle
      1                    // Core (0 or 1)
  );

  xTaskCreatePinnedToCore(
      guiTask,
      "GUI Task",
      GUI_TASK_STACK_SIZE,
      NULL,
      GUI_TASK_PRIORITY,
      &guiTaskHandle,
      0 // Put GUI on core 0 for better performance
  );

  xTaskCreatePinnedToCore(
      meshtasticTask,
      "Meshtastic Task",
      MESHTASTIC_TASK_STACK_SIZE,
      NULL,
      MESHTASTIC_TASK_PRIORITY,
      &meshtasticTaskHandle,
      1);

  xTaskCreatePinnedToCore(
      meshtasticCallbackTask,
      "Meshtastic Callback Task",
      MESHTASTIC_CALLBACK_TASK_STACK_SIZE,
      NULL,
      MESHTASTIC_CALLBACK_TASK_PRIORITY,
      &meshtasticCallbackTaskHandle,
      1);

  xTaskCreatePinnedToCore(
      eepromTask,
      "EEPROM Task",
      EEPROM_TASK_STACK_SIZE,
      NULL,
      EEPROM_TASK_PRIORITY,
      &eepromTaskHandle,
      1);

  xTaskCreatePinnedToCore(
      systemTask,
      "System Task",
      SYSTEM_TASK_STACK_SIZE,
      NULL,
      SYSTEM_TASK_PRIORITY,
      &systemTaskHandle,
      1);

  Serial.println("FreeRTOS tasks created successfully");

} // end setup()

/*****************
 *     LOOP      *
 *****************/

void loop()
{
  // The main loop is now handled by FreeRTOS tasks
  // This loop should remain empty or contain minimal code
  vTaskDelay(pdMS_TO_TICKS(1000));
} // end loop()

/*********************************
 *   DISPLAY & TOUCH FUNCTIONS   *
 *********************************/

// Log if enabled
#if LV_USE_LOG != 0
void my_print(lv_log_level_t level, const char *buf)
{
  LV_UNUSED(level);
  Serial.println(buf);
  gpsSerial.flush();
}
#endif

// LVGL calls this when a rendered image needs to copied to the display
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  /*Call it to tell LVGL you are ready*/
  lv_disp_flush_ready(disp);
}

// Read the touchpad
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
  if (touchscreen.touched())
  {
    TS_Point p = touchscreen.getPoint();
    // Some very basic auto calibration so it doesn't go out of range
    if (p.x < touchScreenMinimumX)
      touchScreenMinimumX = p.x;
    if (p.x > touchScreenMaximumX)
      touchScreenMaximumX = p.x;
    if (p.y < touchScreenMinimumY)
      touchScreenMinimumY = p.y;
    if (p.y > touchScreenMaximumY)
      touchScreenMaximumY = p.y;
    // Map this to the pixel position
    data->point.x = map(p.x, touchScreenMinimumX, touchScreenMaximumX, 1, TFT_WIDTH);  // Touchscreen X calibration
    data->point.y = map(p.y, touchScreenMinimumY, touchScreenMaximumY, 1, TFT_HEIGHT); // Touchscreen Y calibration
    data->state = LV_INDEV_STATE_PRESSED;

#if DEBUG_TOUCH_SCREEN == 1
    Serial.print("Touch x ");
    Serial.print(data->point.x);
    Serial.print(" y ");
    Serial.println(data->point.y);
#endif
  }
  else
  {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// Arduino like analogWrite
// value has to be between 0 and valueMax
void ledcAnalogWrite(uint8_t ledc_channel, uint32_t value, uint32_t valueMax = 255)
{
  // calculate duty, 4095 from 2 ^ 12 - 1
  uint32_t duty = (4095 / valueMax) * min(value, valueMax);

  // write duty to LEDC
  ledcWrite(ledc_channel, duty);
}

/*********************
 *   APP FUNCTIONS   *
 *********************/

// Utility functions
String prefix_zero(int max2digits)
{
  return (max2digits < 10) ? "0" + String(max2digits) : String(max2digits);
}

int make12hr(int time_hr)
{
  return (time_hr > 12) ? time_hr - 12 : time_hr;
}

String am_pm(int time_hr)
{
  return (time_hr > 12) ? String("PM") : String("AM");
}

// Function to convert month number to three char abbreviation
const char *getMonthAbbr(int monthNumber)
{
  // Array of month abbreviations (index 0 unused for 1-based indexing)
  static const char *monthAbbreviations[] = {
      "",    // Index 0 (unused)
      "Jan", // 1
      "Feb", // 2
      "Mar", // 3
      "Apr", // 4
      "May", // 5
      "Jun", // 6
      "Jul", // 7
      "Aug", // 8
      "Sep", // 9
      "Oct", // 10
      "Nov", // 11
      "Dec"  // 12
  };

  // Check for valid month number
  if (monthNumber >= 1 && monthNumber <= 12)
  {
    return monthAbbreviations[monthNumber];
  }
  else
  {
    // Return a default or error string for invalid input
    return "Inv"; // "Invalid" or similar
  }
}

// Function to convert timelib weekday (Sunday=1) to 3-char abbreviation
const char *getDayAbbr(int weekday_num)
{
  // Array of string literals for day abbreviations, indexed from 0.
  // Since Sunday=1, we need to adjust the index.
  const char *day_abbreviations[] = {
      "Sun", // weekday_num = 1
      "Mon", // weekday_num = 2
      "Tue", // weekday_num = 3
      "Wed", // weekday_num = 4
      "Thu", // weekday_num = 5
      "Fri", // weekday_num = 6
      "Sat"  // weekday_num = 7
  };

  // Check for valid input range
  if (weekday_num >= 1 && weekday_num <= 7)
  {
    // Adjust index for 0-based array: weekday_num - 1
    return day_abbreviations[weekday_num - 1];
  }
  else
  {
    // Handle invalid input, e.g., return "Err" or NULL
    return "Err"; // Or any other suitable error indicator
  }
}

/****************************
 *   MESHTASTIC FUNCTIONS   *
 ****************************/

// This callback function will be called whenever the radio connects to a node
// callback activated when responding to mt_request_node_report()
void connected_callback(mt_node_t *node, mt_nr_progress_t progress)
{
  if (not_yet_connected)
    Serial.println("Connected to Meshtastic device!");
  not_yet_connected = false;
}

// This callback function will be called whenever the radio receives a text message
// Now it queues the message for processing by the dedicated callback task
void text_message_callback(uint32_t from, uint32_t to, uint8_t channel, const char *text)
{

  // Create message item for queue
  meshtasticCallbackItem_t item;
  item.from = from;
  item.to = to;
  item.channel = channel;

  // Safely copy text with bounds checking
  if (text != NULL)
  {
    strncpy(item.text, text, MAX_MESHTASTIC_PAYLOAD - 1);
    item.text[MAX_MESHTASTIC_PAYLOAD - 1] = '\0'; // Ensure null termination
  }
  else
  {
    item.text[0] = '\0';
  }

  // Queue the message for processing (non-blocking)
  if (xQueueSend(meshtasticCallbackQueue, &item, 0) != pdTRUE)
  {
    Serial.println("Warning: Meshtastic callback queue full, message dropped");
  }
}

// Parse weather forecast data from input string into eez studio globals
// Example input format: "76.0#1pm,4,0.0#2pm,4,0.0#3pm,7,0.03#4pm,7,0.01#"
int parseWeatherData(char *input)
{

  int ptr, len;
  len = strlen(input);

  // Add this debug code right after len = strlen(input);
  // Serial.print("Original length: "); Serial.println(len);
  // Serial.print("Original input: "); Serial.println(input);

  if (!input)
    return 0;

  // Replace the for loop with this debug version:
  for (ptr = 0; ptr < len; ptr++)
  {
    // Serial.print("Position "); Serial.print(ptr);
    // Serial.print(": '"); Serial.print(input[ptr]); Serial.print("' ");

    if ((input[ptr] == '#') || (input[ptr] == ','))
    {
      // Serial.println("-> REPLACING with null");
      input[ptr] = '\0';
    }
    else
    {
      // Serial.println("-> keeping");
    }
  }

  // // Serial.print("Modified input: ");
  // for(int i = 0; i < len; i++) {  // Use len, not strlen(input)
  //     if(input[i] == '\0')
  //         Serial.print("\\0");
  //     else Serial.print(input[i]);
  // }
  // Serial.println();

  ptr = HOT_PKT_HEADER_OFFSET;
  cur_temp = String(&input[ptr]);
  ptr = ptr + strlen(&input[ptr]) + 1;

  fcast_hr1 = String(&input[ptr]);
  ptr = ptr + fcast_hr1.length() + 1;
  fcast_glyph1 = String(&input[ptr]);
  ptr = ptr + fcast_glyph1.length() + 1;
  fcast_precip1 = String(&input[ptr]);
  ptr = ptr + fcast_precip1.length() + 1;

  fcast_hr2 = String(&input[ptr]);
  ptr = ptr + fcast_hr2.length() + 1;
  fcast_glyph2 = String(&input[ptr]);
  ptr = ptr + fcast_glyph2.length() + 1;
  fcast_precip2 = String(&input[ptr]);
  ptr = ptr + fcast_precip2.length() + 1;

  fcast_hr3 = String(&input[ptr]);
  ptr = ptr + fcast_hr3.length() + 1;
  fcast_glyph3 = String(&input[ptr]);
  ptr = ptr + fcast_glyph3.length() + 1;
  fcast_precip3 = String(&input[ptr]);
  ptr = ptr + fcast_precip3.length() + 1;

  fcast_hr4 = String(&input[ptr]);
  ptr = ptr + fcast_hr4.length() + 1;
  fcast_glyph4 = String(&input[ptr]);
  ptr = ptr + fcast_glyph4.length() + 1;
  fcast_precip4 = String(&input[ptr]);
  ptr = ptr + fcast_precip4.length() + 1;

  return 1;
}


/****************************
 *  USER_ACTIONS FUNCTIONS  *
 ****************************/

// Function to display venue/event data in LVGL table format
void displayVenueEventTable(const char* dataString) {
    // Get the current screen instead of creating a new one
    lv_obj_t * current_screen = lv_scr_act();
    
    lv_obj_t * container = lv_obj_create(current_screen);
    lv_obj_set_pos(container, 0, 40);  // set absolute position
    lv_obj_set_size(container, TFT_HEIGHT, TFT_WIDTH - 40);  // height & width intentionally swapped for landscape orientation
    lv_obj_set_style_bg_color(container, lv_color_black(), LV_PART_MAIN); 
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);  // Remove container border for cleaner look
    
    // Count events to determine table size
    String dataStr = String(dataString);
    int eventCount = 0;
    int pos = 0;
    
    while (pos < dataStr.length()) {
        int delimiterPos = dataStr.indexOf('#', pos);
        if (delimiterPos == -1) break;
        
        String pair = dataStr.substring(pos, delimiterPos);
        if (pair.indexOf(',') != -1) {
            eventCount++;
        }
        pos = delimiterPos + 1;
    }
    
    // Limit to maximum of 12 events
    int maxEvents = min(eventCount, 12);
    if (maxEvents == 0) maxEvents = 1; // Ensure at least 1 row
    
    // Create the table inside the container
    lv_obj_t * table = lv_table_create(container);
    
    // CRITICAL: Set up table structure BEFORE setting size
    lv_table_set_col_cnt(table, 2);
    lv_table_set_row_cnt(table, maxEvents);
    
    // Set column widths FIRST - this allows LVGL to calculate proper table dimensions
    lv_table_set_col_width(table, 0, (TFT_HEIGHT - 30) / 2);  // First column - venue
    lv_table_set_col_width(table, 1, (TFT_HEIGHT - 30) / 2);  // Second column - event
    
    // Now set table size and position
    lv_obj_set_size(table, TFT_HEIGHT - 20, TFT_WIDTH - 60);
    lv_obj_align(table, LV_ALIGN_CENTER, 0, 0);
    
    // Set table main style with rounded corners
    lv_obj_set_style_bg_color(table, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(table, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(table, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(table, 10, LV_PART_MAIN | LV_STATE_DEFAULT);  // Add rounded corners to table
    
    // Style all table cells with consistent font - same as original code pattern
    lv_obj_set_style_bg_color(table, lv_color_hex(0x404040), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(table, lv_color_white(), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(table, &lv_font_montserrat_18, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(table, lv_color_hex(0x808080), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(table, 1, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(table, 3, LV_PART_ITEMS | LV_STATE_DEFAULT);
    
    // Parse the data string and populate the table - same logic as original
    int row = 0; 
    int startPos = 0;
    
    while (startPos < dataStr.length() && row < maxEvents) {
        int delimiterPos = dataStr.indexOf('#', startPos);
        if (delimiterPos == -1) break;
        
        String pair = dataStr.substring(startPos, delimiterPos);
        int commaPos = pair.indexOf(',');
        
        if (commaPos != -1) {
            String venue = pair.substring(0, commaPos);
            String event = pair.substring(commaPos + 1);
            
            // Trim whitespace
            venue.trim();
            event.trim();
            
            // Add to table
            lv_table_set_cell_value(table, row, 0, venue.c_str());
            lv_table_set_cell_value(table, row, 1, event.c_str());
            
            row++;
        }
        
        startPos = delimiterPos + 1;
    }
    
    // If no data was populated, add placeholder
    if (row == 0) {
        lv_table_set_cell_value(table, 0, 0, "No Data");
        lv_table_set_cell_value(table, 0, 1, "Available");
    }
    
    Serial.printf("Table created with %d rows\n", maxEvents);
}

extern "C" void action_display_now_playing(lv_event_t *e) {
    // Use live data if available, otherwise fall back to example data
    const char* dataToDisplay;
    
    if (live_venue_event_data.length() > 0) {
        // Use the live data from Meshtastic HoTPktType 2
        dataToDisplay = live_venue_event_data.c_str();
        Serial.println("Using live venue/event data from Meshtastic");
    } else {
        // Fall back to default data if no live data available
        dataToDisplay = "Sawgrass,NA#Spanish Springs,NA#Lake Sumter,NA#Brownwood,NA#Sawgrass,NA#";
        Serial.println("Using default data - no live Meshtastic data available");
    }
    
    displayVenueEventTable(dataToDisplay);
}