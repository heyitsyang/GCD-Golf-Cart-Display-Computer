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


/********************
 *     INCLUDES     *
 ********************/

#include <lvgl.h>                 //  lvgl@^9.3.0
#include <TFT_eSPI.h>             //  bodmer/TFT_eSPI@^2.5.43
#include <XPT2046_Touchscreen.h>  //  https://github.com/PaulStoffregen/XPT2046_Touchscreen.git


#include "ui/ui.h"            // generated from EEZ Studio
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

#define DRAW_BUF_SIZE (TFT_WIDTH * TFT_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))


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
int value = 0;
bool isDirectionUp = true;


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

  /* Add event handlers */

  // lv_obj_add_event_cb(objects.btn_reset, btn_reset_value_handler, LV_EVENT_ALL, NULL);
  // lv_obj_add_event_cb(objects.btn_random, btn_random_value_handler, LV_EVENT_ALL, NULL);
}

/*****************
 *     LOOP      *
 *****************/

void loop() {

  // * Add app loop code */
  
  // //Check if we want to pause
  // bool isPauseEnabled = lv_obj_has_state(objects.cbx_pause, LV_STATE_CHECKED);

  // if (!isPauseEnabled) {

  //   mockBarValue();
    
  //   // Set bar value
  //   lv_bar_set_value(objects.bar_temperature, value, LV_ANIM_OFF);

  //   //Set value
  //   lv_label_set_text_fmt(objects.label_value_c, "Celsius: %d", value);

  //   //Set value to 2 decimal place
  //   float fahrenheit = ((value * 1.0) * (9.0/5.0)) + 32;
  //   lv_label_set_text_fmt(objects.label_value_f, "Fahrenheit: %.2f%", fahrenheit);
  // }

  
  /* Required for LVGL */
  lv_tick_inc(millis() - lastTick);  //Update the tick timer. Tick is new for LVGL 9.x
  lastTick = millis();
  lv_timer_handler();  //Update the UI
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

//  void mockBarValue() {
//   if (isDirectionUp) {
//     value = value + 1;
//     if (value > 210) {
//       delay(1000);
//       isDirectionUp = false;
//     }
//   } else {
//     value = value - 1;
//     if (value < 0) {
//       delay(1000);
//       isDirectionUp = true;
//     }
//   }
// }

// static void btn_reset_value_handler(lv_event_t *e) {
//   lv_event_code_t code = lv_event_get_code(e);

//   if (code == LV_EVENT_CLICKED) {
//     value = 0;
//     LV_LOG_USER("Clicked");
//   } else if (code == LV_EVENT_VALUE_CHANGED) {
//     LV_LOG_USER("Toggled");
//   }
// }

// static void btn_random_value_handler(lv_event_t *e) {
//   lv_event_code_t code = lv_event_get_code(e);

//   if (code == LV_EVENT_CLICKED) {
//     LV_LOG_USER("Clicked");
//     value = random(0, 210);
//   } else if (code == LV_EVENT_VALUE_CHANGED) {
//     LV_LOG_USER("Toggled");
//   }
// }

