#include "display.h"
#include "config.h"
#include "globals.h"
#include "get_set_vars.h"

// Global display handle
static lv_display_t *display_handle = nullptr;

void initDisplay() {
    // Initialize LVGL
    lv_init();

    // Allocate draw buffer
    draw_buf = new uint8_t[DRAW_BUF_SIZE];

    // Create display
    display_handle = lv_tft_espi_create(TFT_WIDTH, TFT_HEIGHT, draw_buf, DRAW_BUF_SIZE);
    updateDisplayRotation();

    Serial.println("Display initialized");
}

void updateDisplayRotation() {
    if (display_handle != nullptr) {
        if (get_var_flip_screen()) {
            lv_display_set_rotation(display_handle, LV_DISPLAY_ROTATION_90);
        } else {
            lv_display_set_rotation(display_handle, LV_DISPLAY_ROTATION_270);
        }
    }
}

void updateTouchscreenRotation() {
    // Both modes use the same rotation
    touchscreen.setRotation(TOUCH_ROTATION_180);
}

void initTouchscreen() {
    // Initialize touchscreen SPI
    touchscreenSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    touchscreen.begin(touchscreenSpi);

    // Set initial touchscreen rotation based on flip_screen setting
    updateTouchscreenRotation();

    // Create input device
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    Serial.println("Touchscreen initialized");
}

void initBacklight() {
#if ESP_IDF_VERSION_MAJOR == 5
    ledcAttach(TFT_BACKLIGHT_PIN, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
#else
    ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    ledcAttachPin(TFT_BACKLIGHT_PIN, LEDC_CHANNEL_0);
#endif
    
    ledcAnalogWrite(LEDC_CHANNEL_0, 225, MAX_BACKLIGHT_VALUE);
    Serial.println("Backlight initialized");
}

void setBacklight(uint32_t value) {
    ledcAnalogWrite(LEDC_CHANNEL_0, value, MAX_BACKLIGHT_VALUE);
}

void ledcAnalogWrite(uint8_t ledc_channel, uint32_t value, uint32_t valueMax) {
    uint32_t duty = (4095 / valueMax) * min(value, valueMax);
    ledcWrite(ledc_channel, duty);
}

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    if (touchscreen.touched()) {
        // Update touch activity timestamp and reset countdown
        lastTouchActivity = millis();
        set_var_screen_inactivity_countdown((int32_t)SCREEN_INACTIVITY_TIMEOUT_MS);

        TS_Point p = touchscreen.getPoint();

        // Auto calibration
        if (p.x < touchScreenMinimumX) touchScreenMinimumX = p.x;
        if (p.x > touchScreenMaximumX) touchScreenMaximumX = p.x;
        if (p.y < touchScreenMinimumY) touchScreenMinimumY = p.y;
        if (p.y > touchScreenMaximumY) touchScreenMaximumY = p.y;

        // Map to pixel position
        data->point.x = map(p.x, touchScreenMinimumX, touchScreenMaximumX, 1, TFT_WIDTH);
        data->point.y = map(p.y, touchScreenMinimumY, touchScreenMaximumY, 1, TFT_HEIGHT);
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

#if LV_USE_LOG != 0
void my_print(lv_log_level_t level, const char *buf) {
    LV_UNUSED(level);
    Serial.println(buf);
    Serial.flush();
}
#endif