/********************************************************************************************\
*    CYD-EEZ-LVGL Template                                                                   *
*                                                                                            *
*    Note: Knowledge of LVGL is required - see docs at  https://docs.lvgl.io/9.3/            *
*                                                                                            *
*    1. Create a new PlatformIO/VSCode project using this template as main.cpp               *
*    2. Create GUI with EEZ Studio                                                           *
*      - Change EEZ Studio Setting>Build>LVGL_include from lvgl/lvgl.h to lvgl.h             *
*      - Change EEZ Studio Settings>General>LVGL_version to 9.x                              *
*      - Set screen width/height (320/240) in both Settings and in POSITION AND SIZE         *
*      - After creating UI pages, save and BUILD the project in EEZ Studio                   *
*        - This will generate ui code to copy to PlatformIO/VSCode                           *
*      - Copy ui directory from EEZ Studio to the PlatformIO/VSCode project src directory    *
*      - If ui directory code is edited, do not copy over with newly generated from EEZ      *
*      - Add callback routines and app code. Be sure the  ui_init();  is added to setup()    *
*    3. Compile & download the program to the target device using PlatformIO/VSCode          *
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

#include <lvgl.h>                 //  lvgl@^9.3.0
#include <TFT_eSPI.h>             //  bodmer/TFT_eSPI@^2.5.43
#include <XPT2046_Touchscreen.h>  //  https://github.com/PaulStoffregen/XPT2046_Touchscreen.git
#include <TinyGPS++.h>
#include <Timezone.h>             // https://github.com/JChristensen/Timezone
#include <movingAvg.h>
#include <Preferences.h>          // used to save prefeerences in EEPROM
#include <JC_Sunrise.h>           // https://github.com/JChristensen/JC_Sunrise
#include <WiFi.h>

#include "version.h"
#include "ui/ui.h"                // generated from EEZ Studio
#include "get_set_vars.h"         // get & set functions for EEZ Studio vars
#include "prototypes.h"           // declare functions so they can be moved below setup() & loop()
#include "Meshtastic.h"

/********************
 *      DEFINES     *
 ********************/

// enabling any DEBUG bogs down the loop
#define DEBUG_TOUCH_SCREEN 0
#define DEBUG_GPS 0
//#define DEBUG_MESHTASTIC 1
#define MT_DEBUGGING 0            // set to 1 for in-depth debugging

// Touch Screen pins
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

//eSPI touchscreen setRotation() values
#define TOUCH_ROTATION_0 0
#define TOUCH_ROTATION_90 1
#define TOUCH_ROTATION_180 2
#define TOUCH_ROTATION_270 3

#define TOUCH_WIDTH 320     // define touch using LANDSCAPE orientation
#define TOUCH_HEIGHT 240

#define TFT_WIDTH  240      // define this display/driver using PORTRAIT orientation
#define TFT_HEIGHT 320      // (XY origin can differ between displays/drivers)

#define TFT_BACKLIGHT_PIN 21
#define LEDC_CHANNEL_0     0      // use first channel of 16 channels (started from zero)
#define LEDC_BASE_FREQ     5000   // use 5000 Hz as a LEDC base frequency
#define LEDC_TIMER_12_BIT  12     // use 12 bit precission for LEDC timer
#define MAX_BACKLIGHT_VALUE 255

/*LVGL draw into this buffer, 1/10 screen size usually works well. The size is in bytes*/
#define NUM_BUFS 2
#define DRAW_BUF_SIZE (TFT_WIDTH * TFT_HEIGHT * NUM_BUFS / 10 * (LV_COLOR_DEPTH / 8))

#define MT_SERIAL_TX_PIN 22       // TX to Meshtasic
#define MT_SERIAL_RX_PIN 27       // RX from Meshtasic
#define MT_DEV_BAUD_RATE 9600     // Meshtastic device baud rate

#define GPS_RX_PIN 03             // USB connector RX UART0
#define GPS_TX_PIN 01             // USB connector TX UART0
#define GPS_BAUD 9600             // gpsSerial & Serial baud rate
#define GPS_READ_INTERVAL 15000   // in ms

// Needed for JCSunrise library before GPS has signal
#define MY_LATITUDE 28.8522f
#define MY_LONGITUDE -82.0028f


// Send a text message every this many seconds
#define SEND_PERIOD 300
#define MAX_MESHTASTIC_PAYLOAD 237  //max actual payload bytes after subtracting overhead


/******************************
 *    VARIABLES & OBJECTS     *
 ******************************/

/* Display variables */
SPIClass touchscreenSpi = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
uint16_t touchScreenMinimumX = 200, touchScreenMaximumX = 3700, touchScreenMinimumY = 240, touchScreenMaximumY = 3800;
lv_indev_t *indev;      //Touchscreen input device
uint8_t *draw_buf;      //draw_buf is allocated on heap otherwise the static area is too big on ESP32 at compile
uint32_t lastTick = 0;  //Used to track the tick timer

/* App variables */

// GPS and debug serial use the same serial port - GPS uses RX, normal serial debug Serial.print uses TX
// To clarify, gpsSerial is used for gps functions - Serial is used for debug serail printing
HardwareSerial& gpsSerial = Serial;   // create gpsSerial reference to debugSerial so we can use gpsSerial even though it is the same serial port

// Define movingAvg objects
movingAvg avgAzimuthDeg(8), avgSpeed(10);         // value is number of samples

// The TinyGPS++ object
TinyGPSPlus gps;

// GPS Time
int localYear;

// Preferences
Preferences prefs;

// byte gpsMonth, gpsDay, gpsHour, gpsMinute, gpsSecond;
int localMonth, localDay = 0, old_localDay = 0, localHour, localMinute, localSecond, localDayOfWeek;

// time zone & sunrise/sunset definitions
TimeChangeRule mySTD = {"EST", First, Sun, Nov, 2, -300};   //UTC - 5 hours
TimeChangeRule myDST = {"EDT", Second, Sun, Mar, 2, -240};  //UTC - 4 hours
TimeChangeRule *tcr;  // pointer to use to extract TZ abbreviation later
Timezone myTZ(myDST, mySTD);

JC_Sunrise sun {MY_LATITUDE, MY_LONGITUDE, JC_Sunrise::officialZenith};  // set lat & lon for sunrise/sunset calculation

// time variables
time_t localTime, utcTime;
int timesetinterval = 60; //set microcontroller time every 60 seconds

// GPS
String latitude;
String longitude;
String altitude;
float hdop, old_max_hdop;
int old_day_backlight, old_night_backlight;
unsigned long previousGPSms = 0;
float accumDistance;

// the rest are defined in get_set_vars.h as aprt of EEZ Studio integration
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

/*****************
 *     SETUP     *
 *****************/

void setup() {

    /* 
   * UART0 TX/RX lines are used in this manner:
   *   - RX is wired to GPS using serial TTL and bypasses the UART0 completely
   *   - TX is used for serial print messges for debugging and not connected to the GPS
   *     but can be monitored with normal a USB connection to UART0 (USB connecotr)
   */
  
  // Initialize Serial (aka gpsSerial) with the defined RX and TX pins and a baud rate of 9600
  Serial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.print("\ngpsSerial started at 9600 baud rate");

  //Print version on the Serial console
  version = String('v') + String(VERSION);
  String SW_Version = "\nSW ";
  SW_Version += version;
  Serial.println(SW_Version);

  cyd_mac_addr = String(WiFi.macAddress());
  Serial.println(cyd_mac_addr);

  //Intitalize Preferences name space
  prefs.begin("eeprom", false); 

  //Intialize EEPROM stored variables
  max_hdop = prefs.getFloat("max_hdop", 2.5);  // if no such max_hdop, default is the second parameter
  Serial.print("> max_hdop read from eeprom = ");
  Serial.println(max_hdop);
  old_max_hdop = max_hdop;
  
  day_backlight = prefs.getInt("day_backlight", 10);  // if no such day_backlight, default is the second parameter
  Serial.print("> day_backlight read from eeprom = ");
  Serial.println(day_backlight);
  old_day_backlight = day_backlight;

  night_backlight = prefs.getInt("night_backlight", 5);  // if no such night_backlight, default is the second parameter
  Serial.print("> night_backlight  read from eeprom = ");
  Serial.println(night_backlight);
  old_night_backlight = night_backlight;

  espnow_mac_addr = prefs.getString("espnow_mac_addr", "NONE");  // if no such espnow_mac_addr, default is the second parameter
  Serial.print("> espnow_mac_addr read from eeprom = ");
  Serial.println(espnow_mac_addr);
  old_espnow_mac_addr = espnow_mac_addr;

  //Inititalize GPS
  avgAzimuthDeg.begin();
  avgSpeed.begin();

  //Initialise the touchscreen
  touchscreenSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS); // Start second SPI bus for touchscreen
  touchscreen.begin(touchscreenSpi);                                         // Touchscreen init
  touchscreen.setRotation(TOUCH_ROTATION_180);  

  // Initialize LVGL
  String LVGL_Arduino = "LVGL ";
  LVGL_Arduino += String('v') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.println(LVGL_Arduino);

  cur_date = String("NO GPS");  // display this until GPS date is avaialble
  wx_rcv_time = String(" NO DATA YET");  // display until weather report is avaialble

  lv_init();    // initialize LVGL GUI

  draw_buf = new uint8_t[DRAW_BUF_SIZE];
  lv_display_t *disp;
  disp = lv_tft_espi_create(TFT_WIDTH, TFT_HEIGHT, draw_buf, DRAW_BUF_SIZE);
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);     // set display rotation - this is independent of display

  //Initialize the XPT2046 input device driver
  indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);

  Serial.println("LVGL Setup done");

  //Integrate GUI from EEZ
  ui_init();
  
  //Inititalize screen backlight
  #if ESP_IDF_VERSION_MAJOR == 5
    ledcAttach(TFT_BACKLIGHT_PIN, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  #else
    ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    ledcAttachPin(TFT_BACKLIGHT_PIN, LEDC_CHANNEL_0);
  #endif

  ledcAnalogWrite(LEDC_CHANNEL_0, 225, MAX_BACKLIGHT_VALUE);  // set backlight to an initial visible state

  // Initialize Meshtastic
  mt_serial_init(MT_SERIAL_RX_PIN, MT_SERIAL_TX_PIN, MT_DEV_BAUD_RATE);  // start meshtastic serial port

  randomSeed(micros());   // randomise the random() function using micros()

  mt_request_node_report(connected_callback);   // Initial connection to the Meshtastic device

  set_text_message_callback(text_message_callback);  // Register a callback function to be called whenever a text message is received

  manual_reboot = false;
  mesh_comm = true;

} // end setup()


/*****************
 *     LOOP      *
 *****************/

void loop() {

  time_t sunrise_t, sunset_t;
  unsigned long currentGPSms = millis();
  int elapsed_gps_read;
  float incDistance;

  if(manual_reboot == true)    // rebooted from UI
    ESP.restart();
  
  if(mesh_comm != old_mesh_comm) {
    old_mesh_comm = mesh_comm;
    if(mesh_comm == false) {
      mt_serial_end();           // stop the port
      Serial.println("meshSerial disabled");
    }
    else {
      mt_serial_init(MT_SERIAL_RX_PIN, MT_SERIAL_TX_PIN, MT_DEV_BAUD_RATE);  // start meshtastic serial port
      Serial.println("meshSerial enabled");
    }
  }
    

  elapsed_gps_read = currentGPSms - previousGPSms;
  if (elapsed_gps_read >= GPS_READ_INTERVAL) {
    previousGPSms = currentGPSms; // Update the last execution time
    while (gpsSerial.available() > 0)     // pass any character(s) in rx buffer to the GPS library
      gps.encode(gpsSerial.read());
    if (gps.location.isUpdated()) {       // check if end of valid NMEA sentence - typically once per second

      // set the Time to the latest GPS reading
      if (gps.date.isValid() && gps.time.isValid()) {
        setTime(gps.time.hour(), gps.time.minute(), gps.time.second(), gps.date.day(), gps.date.month(), gps.date.year());
        utcTime = now();
        localTime = myTZ.toLocal(utcTime, &tcr);
        localYear = year(localTime);
        localMonth = month(localTime);
        localDay = day(localTime);
        localHour = hour(localTime);
        localMinute = minute(localTime);
        localSecond = second(localTime);
        localDayOfWeek = weekday(localTime);
      }

      latitude = String(gps.location.lat(), 6);
      longitude = String(gps.location.lng(), 6);
      altitude = String(gps.altitude.meters(), 2);
      hdop = gps.hdop.value() / 100.0;
      sats_hdop = String(gps.satellites.value()) + "/" + String(hdop, 2);
      cur_date = String(getDayAbbr(localDayOfWeek)) + ", " + String(getMonthAbbr(localMonth)) + " " + String(localDay);
      hhmm_str = String(make12hr(localHour)) + ":" + String(prefix_zero(localMinute));
      hhmmss_str = String(make12hr(localHour)) + ":" + String(prefix_zero(localMinute)) + String(prefix_zero(localSecond));
      am_pm_str = am_pm(localHour);

      if (hdop < max_hdop) {  // if not reliable data, stay with old values
        avg_speed = avgSpeed.reading(gps.speed.mph());
        heading = String(gps.cardinal(avgAzimuthDeg.reading(gps.course.deg())));
      }

      /*
      * Set the display's backlight
      */
      if (localDay != old_localDay)      // get new sunrise & sunset time
        sun.calculate(localTime, tcr->offset, sunrise_t, sunset_t);

      if ((localTime > sunrise_t) && (localTime < sunset_t)) {
        ledcAnalogWrite(LEDC_CHANNEL_0, ((day_backlight*20) + 55), MAX_BACKLIGHT_VALUE);
        // Serial.print("now using day_backlight value: ");
        // Serial.println(day_backlight);
      }
      else {
        ledcAnalogWrite(LEDC_CHANNEL_0, (night_backlight*20), MAX_BACKLIGHT_VALUE);
        // Serial.print("now using night_backlight value: ");
        // Serial.println(night_backlight);
      }

      /*
       * Approximate distance traveleled
       */
      incDistance = avg_speed * elapsed_gps_read / 3600000;
      accumDistance = accumDistance + incDistance;
      
      #if DEBUG_GPS == 1                           // comment out lines not necessary for your debug
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
      Serial.print("ALT (min)= ");
      Serial.println(altitude);
      Serial.print("Sats/HDOP = "); 
      Serial.println(sats_hdop);
      Serial.print("Local time: ");
      Serial.println(String(localYear) + "-" + String(localMonth) + "-" + String(localDay) + ", " + String(localHour) + ":" + String(localMinute) + ":" + String(localSecond));
      Serial.println(cur_date + " " + hhmm_str);
      #endif

    }
  }

  /*
   * Write values to eeprom only if changed
   */
  if(day_backlight != old_day_backlight) {
    ledcAnalogWrite(LEDC_CHANNEL_0, ((day_backlight*20) + 55), MAX_BACKLIGHT_VALUE);
    prefs.putInt("day_backlight", day_backlight);
    old_day_backlight = day_backlight;
    Serial.print("< day_backlight saved to eeprom:");
    Serial.println(day_backlight);
  }

  if(night_backlight != old_night_backlight) {
    ledcAnalogWrite(LEDC_CHANNEL_0, (night_backlight*20), MAX_BACKLIGHT_VALUE);
    prefs.putInt("night_backlight", night_backlight);
    old_night_backlight = night_backlight;
    Serial.print("< night_backlight saved to eeprom:");
    Serial.println(night_backlight);
  }
  
  if(max_hdop != old_max_hdop) {
    prefs.putFloat("max_hdop", max_hdop);
    old_max_hdop = max_hdop;
    Serial.print("> max_hdop saved to eeprom:");
    Serial.println(max_hdop);
  }

  if(espnow_mac_addr != old_espnow_mac_addr) {
    prefs.putString("espnow_mac_addr", espnow_mac_addr);
    old_espnow_mac_addr = espnow_mac_addr;
    Serial.print("> espnow_mac_addr saved to eeprom:");
    Serial.println(espnow_mac_addr);
  }


  // Record the time that this loop began (in milliseconds since the device booted)
  uint32_t loop_start = millis();

  // Run the Meshtastic loop, and see if it's able to send requests to the device yet
  bool can_send = mt_loop(loop_start);

  // If we can send, and it's time to do so, send a text message and schedule the next one.
  if (can_send && loop_start >= next_send_time) {
    
    // Change this to a specific node number if you want to send to just one node
    uint32_t dest = BROADCAST_ADDR; 
    // Change this to another input if you want to send on a different channel
    // Remember routine Meshtastic telemetry is only sent on channel 0
    uint8_t channel_index = 0;   // For the golf cart:  0 = Telemetry, 1 = TheVillages, 2 = LongFast

    mt_send_text("Hello, world from CYD!", dest, channel_index);

    next_send_time = loop_start + SEND_PERIOD * 1000;
  }


  /* Required for LVGL */
  lv_tick_inc(millis() - lastTick);  //Update the tick timer. Tick is new for LVGL 9.x
  lastTick = millis();
  lv_timer_handler();   // Update the UI for LVGL
  ui_tick();            // Update the UI for EEZ Studio Flow
  delay(5);             // not sure why this is necessary

}  // end loop()




/*********************************
 *   DISPLAY & TOUCH FUNCTIONS   *
 *********************************/

// Log if enabled
#if LV_USE_LOG != 0
void my_print(lv_log_level_t level, const char *buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  gpsSerial.flush();
}
#endif

// LVGL calls this when a rendered image needs to copied to the display
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  /*Call it to tell LVGL you are ready*/
  lv_disp_flush_ready(disp);
}

// Read the touchpad
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
  if (touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    // Some very basic auto calibration so it doesn't go out of range
    if (p.x < touchScreenMinimumX) touchScreenMinimumX = p.x;
    if (p.x > touchScreenMaximumX) touchScreenMaximumX = p.x;
    if (p.y < touchScreenMinimumY) touchScreenMinimumY = p.y;
    if (p.y > touchScreenMaximumY) touchScreenMaximumY = p.y;
    // Map this to the pixel position
    data->point.x = map(p.x, touchScreenMinimumX, touchScreenMaximumX, 1, TFT_WIDTH); // Touchscreen X calibration
    data->point.y = map(p.y, touchScreenMinimumY, touchScreenMaximumY, 1, TFT_HEIGHT); // Touchscreen Y calibration
    data->state = LV_INDEV_STATE_PRESSED;

    #if DEBUG_TOUCH_SCREEN == 1
    Serial.print("Touch x ");
    Serial.print(data->point.x);
    Serial.print(" y ");
    Serial.println(data->point.y);
    #endif

  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// Arduino like analogWrite
// value has to be between 0 and valueMax
void ledcAnalogWrite(uint8_t ledc_channel, uint32_t value, uint32_t valueMax = 255) {
  // calculate duty, 4095 from 2 ^ 12 - 1
  uint32_t duty = (4095 / valueMax) * min(value, valueMax);

  // write duty to LEDC
  ledcWrite(ledc_channel, duty);
}

/*********************
 *   APP FUNCTIONS   *
 *********************/

 // Utility functions
String prefix_zero(int max2digits) {
  return (max2digits < 10) ? "0" + String(max2digits) : String(max2digits);
}

int make12hr(int time_hr) {
  return (time_hr > 12) ? time_hr - 12 : time_hr;
}

String am_pm(int time_hr) {
  return (time_hr > 12) ? String("PM") : String("AM");
}

// Function to convert month number to three char abbreviation
const char* getMonthAbbr(int monthNumber) {
    // Array of month abbreviations (index 0 unused for 1-based indexing)
    static const char* monthAbbreviations[] = {
        "",     // Index 0 (unused)
        "Jan",  // 1
        "Feb",  // 2
        "Mar",  // 3
        "Apr",  // 4
        "May",  // 5
        "Jun",  // 6
        "Jul",  // 7
        "Aug",  // 8
        "Sep",  // 9
        "Oct",  // 10
        "Nov",  // 11
        "Dec"   // 12
    };

    // Check for valid month number
    if (monthNumber >= 1 && monthNumber <= 12) {
        return monthAbbreviations[monthNumber];
    } else {
        // Return a default or error string for invalid input
        return "Inv"; // "Invalid" or similar
    }
}

// Function to convert timelib weekday (Sunday=1) to 3-char abbreviation
const char* getDayAbbr(int weekday_num) {
    // Array of string literals for day abbreviations, indexed from 0.
    // Since Sunday=1, we need to adjust the index.
    const char* day_abbreviations[] = {
        "Sun", // weekday_num = 1
        "Mon", // weekday_num = 2
        "Tue", // weekday_num = 3
        "Wed", // weekday_num = 4
        "Thu", // weekday_num = 5
        "Fri", // weekday_num = 6
        "Sat"  // weekday_num = 7
    };

    // Check for valid input range
    if (weekday_num >= 1 && weekday_num <= 7) {
        // Adjust index for 0-based array: weekday_num - 1
        return day_abbreviations[weekday_num - 1];
    } else {
        // Handle invalid input, e.g., return "Err" or NULL
        return "Err"; // Or any other suitable error indicator
    }
}

/****************************
 *   MESHTASTIC FUNCTIONS   *
 ****************************/

// This callback function will be called whenever the radio connects to a node
// callback activated when responding to mt_request_node_report()
void connected_callback(mt_node_t *node, mt_nr_progress_t progress) {
  if (not_yet_connected)   
    Serial.println("Connected to Meshtastic device!");
  not_yet_connected = false;
}

// This callback function will be called whenever the radio receives a text message
void text_message_callback(uint32_t from, uint32_t to,  uint8_t channel, const char* text) {

  char scratch[MAX_MESHTASTIC_PAYLOAD] = {0};   //use as scratchpad buffer

  // Do your own thing here. This example just prints the message to the serial console.
  Serial.print("Received a text message on channel: ");
  Serial.print(channel);
  Serial.print(" from: ");
  Serial.print(from);
  Serial.print(" to: ");
  Serial.print(to);
  Serial.print(" message: ");
  Serial.println(text);
  if (to == 0xFFFFFFFF){
    Serial.println("This is a BROADCAST message.");
  } else if (to == my_node_num){
    Serial.println("This is a DM to me!");
  } else {
    Serial.println("This is a DM to someone else.");
    }


  // coded telemetry message
  // example wx packet:   |#01#76#1pm,4,0.0#2pm,4,0.0#3pm,7,0.03#4pm,7,0.01#
  // where | is start of pkt character for a HoT pkt, 01 is telemetery pkt type (01 = wx), and the rest is wx pkt payload
  if (text != NULL && text[0] == '|') {
    Serial.println("Received a HoT pkt");
    // Now, combine the 3rd and 4th characters into a single integer.
    // Memory-efficient method: Perform the conversion manually.
    // '0' is 48 in ASCII, '1' is 49. '1' - '0' = 1.
    int HotPktType = (text[2] - '0') * 10 + (text[3] - '0');

    // Handle potential invalid input for robustness
    if (text[2] < '0' || text[2] > '9' || text[3] < '0' || text[3] > '9') {
        Serial.print("Malformed HotPktType:  ");
        Serial.println(HotPktType);
    } else {

      // Process the extracted number using a switch statement
      switch (HotPktType) {
          case 1:   // wx packet
              Serial.println("WX packet received");
              wx_rcv_time = cur_date + "  " + hhmm_str + am_pm_str;
              parseWeatherData((char *)text);
              break;
          // Add more cases as needed
          default:
              Serial.print("Unrecognized HotPktType:  ");
              Serial.println(HotPktType);
              break;
      }
    }
  }
  ui_tick();            // Update the UI for EEZ Studio Flow
}



// Parse weather forecast data from input string into eez studio globals
// Input format: "76.0#1pm,4,0.0#2pm,4,0.0#3pm,7,0.03#4pm,7,0.01#"
// Returns: number of hourly entries parsed (0-MAX_HOURLY_DATA)
int parseWeatherData(char* input) {

    int ptr, len;
    len = strlen(input);

    // Add this debug code right after len = strlen(input);
    // Serial.print("Original length: "); Serial.println(len);
    // Serial.print("Original input: "); Serial.println(input);

    if (!input)
        return 0;

    // Replace the for loop with this debug version:
    for(ptr = 0; ptr < len; ptr++) {
        // Serial.print("Position "); Serial.print(ptr); 
        // Serial.print(": '"); Serial.print(input[ptr]); Serial.print("' ");
        
        if ((input[ptr] == '#') || (input[ptr] == ',')) {
            // Serial.println("-> REPLACING with null");
            input[ptr] = '\0';
        } else {
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

    #define HOT_PKT_HEADER_OFFSET 5
    
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