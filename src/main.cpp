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
#include <Timezone.h>
#include <movingAvg.h>


#include "ui/ui.h"            // generated from EEZ Studio
#include "get_set_vars.h"     // get & set functions for EEZ Studio vars
#include "prototypes.h"       // declare functions so they can be moved below setup() & loop()

/********************
 *      DEFINES     *
 ********************/

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

#define TFT_WIDTH  240      // for some reason display library
#define TFT_HEIGHT 320      // is defined using PORTRAIT orientation

/*LVGL draw into this buffer, 1/10 screen size usually works well. The size is in bytes*/

#define NUM_BUFS 2
#define DRAW_BUF_SIZE (TFT_WIDTH * TFT_HEIGHT * NUM_BUFS / 10 * (LV_COLOR_DEPTH / 8))

// Define the RX, TX pins & baud rate for GPS serial data
#define GPS_RX_PIN 22
#define GPS_TX_PIN 27
#define GPS_BAUD 9600


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
// int value = 0;
// bool isDirectionUp = true;



// Create an instance of the HardwareSerial class for CYD Serial Port 2
HardwareSerial gpsSerial(2);

// Define movingAvg objects
movingAvg avgAzimuthDeg(8), avgSpeed(10);         // value is number of samples

// The TinyGPS++ object
TinyGPSPlus gps;

// GPS Time
int localYear;
// byte gpsMonth, gpsDay, gpsHour, gpsMinute, gpsSecond;
int localMonth, localDay, localHour, localMinute, localSecond, localDayOfWeek;

// time zone definitions
TimeChangeRule mySTD = {"EST", First, Sun, Nov, 2, -300};   //UTC - 5 hours
TimeChangeRule myDST = {"EDT", Second, Sun, Mar, 2, -240};  //UTC - 4 hours
TimeChangeRule *tcr;  // pointer to use to extract TZ abbreviation later
Timezone myTZ(myDST, mySTD);

// time variables
time_t localTime, utcTime;
int timesetinterval = 60; //set microcontroller time every 60 seconds

// String cur_date;
// String hhmm_t;
// String hhmmss_t;
// String strAmPm;
String latitude;
String longitude;
String altitude;
// String speed;
// String heading;
String hdop;
String satellites;

/*****************
 *     SETUP     *
 *****************/

void setup() {

  //Some basic info on the Serial console
  String LVGL_Arduino = "LVGL";
  LVGL_Arduino += String('v') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.begin(115200);
  Serial.println(LVGL_Arduino);

  //Initialise the touchscreen
  touchscreenSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS); // Start second SPI bus for touchscreen
  touchscreen.begin(touchscreenSpi);                                         // Touchscreen init
  touchscreen.setRotation(TOUCH_ROTATION_180);               //set eSPI touchscreen rotation - independent of display rotation

  avgAzimuthDeg.begin();
  avgSpeed.begin();

  // Start Serial 2 with the defined RX and TX pins and a baud rate of 9600
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println("Serial 2 started at 9600 baud rate");

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

  Serial.println("LVGL Setup done");

  //Integrate GUI from EEZ
  ui_init();
}

/*****************
 *     LOOP      *
 *****************/

void loop() {

  while (gpsSerial.available() > 0)     // pass any character(s) in rx buffer to the GPS library
    gps.encode(gpsSerial.read());
  if (gps.location.isUpdated()) {       // check if end of valid NMEA sentence - typically once per second

    // set the Time to the latest GPS reading
    if (gps.date. isValid() && gps.time.isValid()) {
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
  
    Serial.print("LAT: ");
    latitude = String(gps.location.lat(), 6);
    Serial.println(latitude);
    Serial.print("LONG: "); 
    longitude = String(gps.location.lng(), 6);
    Serial.println(longitude);

    speed = avgSpeed.reading(gps.speed.mph());
    if(speed < 3) {
      speed = 0;
      heading = String("");
    } else {
      heading = String(gps.cardinal(avgAzimuthDeg.reading(gps.course.deg())));
    }
    Serial.print("SPEED (mph) = ");
    Serial.println(speed);
    Serial.print("DIRECTION = ");
    Serial.println(heading);

    Serial.print("ALT (min)= ");
    altitude = String(gps.altitude.meters(), 2);
    Serial.println(altitude);
    Serial.print("HDOP = "); 
    hdop = String(gps.hdop.value() / 100.0, 2);
    Serial.println(hdop);
    Serial.print("Satellites = "); 
    satellites = String(gps.satellites.value());
    Serial.println(satellites);

    Serial.print("Local time: ");
    Serial.println(String(localYear) + "-" + String(localMonth) + "-" + String(localDay) + ", " + String(localHour) + ":" + String(localMinute) + ":" + String(localSecond));
    cur_date = String(getDayAbbr(localDayOfWeek)) + ", " + String(getMonthAbbr(localMonth)) + " " + String(localDay);
    Serial.println(cur_date);
    hhmm_t = String(make12hr(localHour)) + ":" + String(prefix_zero(localMinute));
    hhmmss_t = String(make12hr(localHour)) + ":" + String(prefix_zero(localMinute)) + String(prefix_zero(localSecond));
    Serial.println(hhmm_t);
    Serial.println("");
    str_am_pm = am_pm(localHour);

  }

  
  /* Required for LVGL */
  lv_tick_inc(millis() - lastTick);  //Update the tick timer. Tick is new for LVGL 9.x
  lastTick = millis();
  lv_timer_handler();   //Update the UI for LVGL
  ui_tick();            // Update the UI for EEZ Studio Flow
  delay(5);
}

/*********************************
 *   DISPLAY & TOUCH FUNCTIONS   *
 *********************************/

// Log if enabled
#if LV_USE_LOG != 0
void my_print(lv_log_level_t level, const char *buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
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

    Serial.print("Touch x ");
    Serial.print(data->point.x);
    Serial.print(" y ");
    Serial.println(data->point.y);

  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

/*********************
 *   APP FUNCTIONS   *
 *********************/

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