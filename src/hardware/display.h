#ifndef DISPLAY_H
#define DISPLAY_H

#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

void initDisplay();
void initTouchscreen();
void initBacklight();
void initSpeaker();
void beep(int numBeeps, uint32_t frequency, uint32_t duration, uint32_t pauseMs);

// Predefined tone functions
void tone_startup();
void tone_sleep();
void tone_message();
void tone_alert();
void tone_urgent();
void tone_confirm();
void tone_click();
void tone_error();

void setBacklight(uint32_t value);
void updateDisplayRotation();
void updateTouchscreenRotation();
void ledcAnalogWrite(uint8_t ledc_channel, uint32_t value, uint32_t valueMax = 255);
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data);

#if LV_USE_LOG != 0
void my_print(lv_log_level_t level, const char *buf);
#endif

#endif // DISPLAY_H