#include "display.h"
#include "config.h"
#include "globals.h"
#include "get_set_vars.h"

// Beep control variables
static int beepCount = 0;
static int targetBeeps = 0;
static bool beepOn = false;
static TimerHandle_t beepTimer = NULL;
static uint32_t currentBeepFrequency = BEEP_FREQUENCY_HZ;
static uint32_t currentBeepDuration = BEEP_DURATION_MS;
static uint32_t currentBeepPause = 200;
static int32_t currentBeepVolume = 10;  // Local copy for timer callback


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

        if (use_touch_calibration) {
            // Use calibration coefficients from EEPROM with -90 degree rotation adjustment
            // Calibration was done on 240x320 portrait screen
            // Calculate calibrated position in portrait orientation
            int cal_x = (int)(touch_alpha_x * p.x + touch_beta_x * p.y + touch_delta_x);
            int cal_y = (int)(-touch_alpha_y * p.x - touch_beta_y * p.y + (240 - touch_delta_y));

            // Apply -90 degree rotation for landscape: new_x = y, new_y = x (inverted Y)
            data->point.x = cal_y;
            data->point.y = cal_x;
        } else {
            // Fall back to auto calibration using map()
            if (p.x < touchScreenMinimumX) touchScreenMinimumX = p.x;
            if (p.x > touchScreenMaximumX) touchScreenMaximumX = p.x;
            if (p.y < touchScreenMinimumY) touchScreenMinimumY = p.y;
            if (p.y > touchScreenMaximumY) touchScreenMaximumY = p.y;

            // Map to pixel position
            data->point.x = map(p.x, touchScreenMinimumX, touchScreenMaximumX, 1, TFT_WIDTH);
            data->point.y = map(p.y, touchScreenMinimumY, touchScreenMaximumY, 1, TFT_HEIGHT);
        }

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

// Beep timer callback function
void beepTimerCallback(TimerHandle_t xTimer) {
    if (beepOn) {
        // Turn off beep
        ledcWrite(SPEAKER_LEDC_CHANNEL, 0);
        beepOn = false;
        beepCount++;

        if (beepCount < targetBeeps) {
            // Schedule next beep after pause
            xTimerChangePeriod(beepTimer, pdMS_TO_TICKS(currentBeepPause), 0);
        } else {
            // All beeps completed, stop timer
            xTimerStop(beepTimer, 0);
            beepCount = 0;
            targetBeeps = 0;
        }
    } else {
        // Turn on beep if we haven't reached target count
        if (beepCount < targetBeeps) {
            // Check volume - mute if volume is 0 (use local copy to avoid race conditions)
            if (currentBeepVolume <= 0) {
                ledcWrite(SPEAKER_LEDC_CHANNEL, 0);
            } else {
                // Calculate duty cycle based on volume (0-100% -> 0%-100%)
                uint32_t maxDuty = (1 << SPEAKER_LEDC_TIMER_BIT) / 2; // 50% max duty cycle
                uint32_t volumeDuty = (maxDuty * currentBeepVolume) / 100;
                ledcWrite(SPEAKER_LEDC_CHANNEL, volumeDuty);
            }
            beepOn = true;

            // Schedule beep off after beep duration
            xTimerChangePeriod(beepTimer, pdMS_TO_TICKS(currentBeepDuration), 0);
        }
    }
}

void initSpeaker() {
    // Initialize LEDC for speaker
#if ESP_IDF_VERSION_MAJOR == 5
    ledcAttach(SPEAKER_PIN, BEEP_FREQUENCY_HZ, SPEAKER_LEDC_TIMER_BIT);
#else
    ledcSetup(SPEAKER_LEDC_CHANNEL, BEEP_FREQUENCY_HZ, SPEAKER_LEDC_TIMER_BIT);
    ledcAttachPin(SPEAKER_PIN, SPEAKER_LEDC_CHANNEL);
#endif

    // Create beep timer
    beepTimer = xTimerCreate("BeepTimer", pdMS_TO_TICKS(BEEP_DURATION_MS),
                             pdFALSE, NULL, beepTimerCallback);

    Serial.println("Speaker initialized");
}

void beep(int numBeeps, uint32_t frequency, uint32_t duration, uint32_t pauseMs) {
    if (numBeeps <= 0 || beepTimer == NULL) {
        return;
    }

    // Update frequency if different
    if (frequency != currentBeepFrequency) {
        currentBeepFrequency = frequency;
#if ESP_IDF_VERSION_MAJOR == 5
        ledcChangeFrequency(SPEAKER_PIN, frequency, SPEAKER_LEDC_TIMER_BIT);
#else
        ledcSetup(SPEAKER_LEDC_CHANNEL, frequency, SPEAKER_LEDC_TIMER_BIT);
#endif
    }

    // Update timing parameters and copy current volume for timer callback
    currentBeepDuration = duration;
    currentBeepPause = pauseMs;
    currentBeepVolume = speaker_volume;  // Safe copy for timer callback

    // Stop any ongoing beep sequence
    xTimerStop(beepTimer, 0);
    ledcWrite(SPEAKER_LEDC_CHANNEL, 0);

    // Reset counters and start new sequence
    beepCount = 0;
    targetBeeps = numBeeps;
    beepOn = false;

    // Start the beep sequence
    beepTimerCallback(beepTimer);
}

