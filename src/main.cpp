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

#include "version.h"
#include "ui/ui.h"            // generated from EEZ Studio
#include "get_set_vars.h"     // get & set functions for EEZ Studio vars
#include "prototypes.h"       // declare functions so they can be moved below setup() & loop()

/********************
 *      DEFINES     *
 ********************/

#define DEBUG 1               // enable/disable gpsSerialPrint() messages: 0=none, 1=most, 2=1+touch xy data


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

// Define the RX, TX pins & baud rate for GPS gpsSerial data
#define GPS_UART_NUM 0      // select either UART 0 or 2 only

#if GPS_UART_NUM == 0
  #define GPS_RX_PIN 03     // USB connector RX UART0
  #define GPS_TX_PIN 01     // USB connector TX UART0
#else
  #define GPS_RX_PIN 22     // RX for UART2
  #define GPS_TX_PIN 27     // TX for UART2
#endif

#define GPS_BAUD 9600

#define MY_LATiTUDE 28.8522f
#define MY_LONGITUDE -82.0028f


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

// Create an instance of the HardwaregpsSerial class for a CYD gpsSerial Port
HardwareSerial gpsSerial(GPS_UART_NUM);  //ESP32 maps the GPIOs designated later to UART0

// Define movingAvg objects
movingAvg avgAzimuthDeg(8), avgSpeed(10);         // value is number of samples

// The TinyGPS++ object
TinyGPSPlus gps;

// GPS Time
int localYear;

// Preferences
Preferences prefs;

// byte gpsMonth, gpsDay, gpsHour, gpsMinute, gpsSecond;
int localMonth, localDay, old_localDay = 0, localHour, localMinute, localSecond, localDayOfWeek;

// time zone & sunrise/sunset definitions
TimeChangeRule mySTD = {"EST", First, Sun, Nov, 2, -300};   //UTC - 5 hours
TimeChangeRule myDST = {"EDT", Second, Sun, Mar, 2, -240};  //UTC - 4 hours
TimeChangeRule *tcr;  // pointer to use to extract TZ abbreviation later
Timezone myTZ(myDST, mySTD);

JC_Sunrise sun {MY_LATiTUDE, MY_LONGITUDE, JC_Sunrise::officialZenith};  // set lat & lon for sunrise/sunset calculation

// time variables
time_t localTime, utcTime;
int timesetinterval = 60; //set microcontroller time every 60 seconds

String latitude;
String longitude;
String altitude;
float hdop, old_max_hdop;
int old_day_backlight, old_night_backlight;

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

/*****************
 *     SETUP     *
 *****************/

void setup() {

  //Print some basic info on the gpsSerial console
  version = String('v') + String(VERSION);
  String SW_Version = "\nSW ";
  SW_Version += version;

  String LVGL_Arduino = "LVGL ";
  LVGL_Arduino += String('v') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

  /* 
   * UART0 TX/RX lines are used in this manner:
   *   - RX is wired to GPS using serial TTL and bypasses the UART0 completely
   *   - TX is used for serial print messges for debugging and not connected to the GPS
   *     but can be monitored with normal a USB connection to UART0 (USB connecotr)
   */
  
  // Start gpsSerial with the defined RX and TX pins and a baud rate of 9600
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  gpsSerial.print("\ngpsSerial set by GPS_UART_NUM started at 9600 baud rate");

  gpsSerial.println(SW_Version);
  gpsSerial.println(LVGL_Arduino);

  //Initialise the touchscreen
  touchscreenSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS); // Start second SPI bus for touchscreen
  touchscreen.begin(touchscreenSpi);                                         // Touchscreen init
  touchscreen.setRotation(TOUCH_ROTATION_180);  

  //Inititalize GPS
  avgAzimuthDeg.begin();
  avgSpeed.begin();

  //Intitalize Preferences name space
  prefs.begin("eeprom", false); 
  
  //Intialize EEPROM stored variables
  max_hdop = prefs.getFloat("max_hdop", 2.5);  // if no such max_hdop, default is the second parameter
  gpsSerial.print("> max_hdop read from eeprom = ");
  gpsSerial.println(max_hdop);
  old_max_hdop = max_hdop;
  
  day_backlight = prefs.getInt("day_backlight", 255);  // if no such day_backlight, default is the second parameter
  gpsSerial.print("> day_backlight read from eeprom = ");
  gpsSerial.println(day_backlight);
  old_day_backlight = day_backlight;

  night_backlight = prefs.getInt("night_backlight", 128);  // if no such night_backlight, default is the second parameter
  gpsSerial.print("> night_backlight  read from eeprom = ");
  gpsSerial.println(night_backlight);
  old_night_backlight = night_backlight;

  //Initialise LVGL GUI
  lv_init();

  draw_buf = new uint8_t[DRAW_BUF_SIZE];
  lv_display_t *disp;
  disp = lv_tft_espi_create(TFT_WIDTH, TFT_HEIGHT, draw_buf, DRAW_BUF_SIZE);
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);     // set display rotation - this is independent of display

  //Initialize the XPT2046 input device driver
  indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);

  gpsSerial.println("LVGL Setup done");

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

} // end setup()


/*****************
 *     LOOP      *
 *****************/

void loop() {

  time_t sunrise_t, sunset_t;

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

    if (localDay != old_localDay)      // get new sunrise & sunset time
      sun.calculate(localTime, tcr->offset, sunrise_t, sunset_t);

    if ((localTime > sunset_t) && (localTime < sunrise_t)) {
      ledcAnalogWrite(LEDC_CHANNEL_0, night_backlight, MAX_BACKLIGHT_VALUE);
      gpsSerial.print("night_backlight set: ");
      gpsSerial.println(night_backlight);
    }
    else {
      ledcAnalogWrite(LEDC_CHANNEL_0, day_backlight, MAX_BACKLIGHT_VALUE);
      gpsSerial.print("day_backlight set: ");
      gpsSerial.println(night_backlight);
    }
    
    #if DEBUG > 0
    // vars not dependent on GPS signal
    gpsSerial.print("\nsunrise:");
    gpsSerial.print(ctime(&sunrise_t));
    gpsSerial.print("sunset:");
    gpsSerial.print(ctime(&sunset_t));
    gpsSerial.print("Max HDOP = ");
    gpsSerial.println(max_hdop);

    // vars dependent on GPS signal
    gpsSerial.println("");
    gpsSerial.print("LAT: ");
    gpsSerial.println(latitude);
    gpsSerial.print("LONG: "); 
    gpsSerial.println(longitude);
    gpsSerial.print("AVG_SPEED (mph) = ");
    gpsSerial.println(avg_speed);
    gpsSerial.print("DIRECTION = ");
    gpsSerial.println(heading);
    gpsSerial.print("ALT (min)= ");
    gpsSerial.println(altitude);
    gpsSerial.print("Sats/HDOP = "); 
    gpsSerial.println(sats_hdop);
    gpsSerial.print("Local time: ");
    gpsSerial.println(String(localYear) + "-" + String(localMonth) + "-" + String(localDay) + ", " + String(localHour) + ":" + String(localMinute) + ":" + String(localSecond));
    gpsSerial.println(cur_date + " " + hhmm_str);
    #endif

  }

  /*
   * Write values to eeprom only if changed
   */
  if(day_backlight != old_day_backlight) {
    ledcAnalogWrite(LEDC_CHANNEL_0, day_backlight, MAX_BACKLIGHT_VALUE);
    prefs.putInt("day_backlight", day_backlight);
    old_day_backlight = day_backlight;
    gpsSerial.print("< day_backlight saved to eeprom:");
    gpsSerial.println(day_backlight);
  }

  if(night_backlight != old_night_backlight) {
    ledcAnalogWrite(LEDC_CHANNEL_0, night_backlight, MAX_BACKLIGHT_VALUE);
    prefs.putInt("night_backlight", night_backlight);
    old_night_backlight = night_backlight;
    gpsSerial.print("< night_backlight saved to eeprom:");
    gpsSerial.println(night_backlight);
  }
  
  if(max_hdop != old_max_hdop) {
    prefs.putFloat("max_hdop", max_hdop);
    old_max_hdop = max_hdop;
    gpsSerial.print("> max_hdop saved to eeprom:");
    gpsSerial.println(max_hdop);
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
  gpsSerial.println(buf);
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

    #if DEBUG == 2
    gpsSerial.print("Touch x ");
    gpsSerial.print(data->point.x);
    gpsSerial.print(" y ");
    gpsSerial.println(data->point.y);
    #endif

  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// Arduino like analogWrite
// value has to be between 0 and valueMax
void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255) {
  // calculate duty, 4095 from 2 ^ 12 - 1
  uint32_t duty = (4095 / valueMax) * min(value, valueMax);

  // write duty to LEDC
  ledcWrite(channel, duty);
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