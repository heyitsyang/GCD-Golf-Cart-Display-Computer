#include "display.h"
#include "config.h"
#include "globals.h"
#include "get_set_vars.h"

// Beep control variables
static int beepCount = 0;
static int targetBeeps = 0;
static bool beepOn = false;
static hw_timer_t *beepTimer = NULL;
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
            lv_display_set_rotation(display_handle, LV_DISPLAY_ROTATION_270);
        } else {
            lv_display_set_rotation(display_handle, LV_DISPLAY_ROTATION_90);
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

// Hardware timer ISR - must be fast, no Serial.printf allowed
void IRAM_ATTR beepTimerISR() {
    if (beepOn) {
        // Turn off beep
        ledcWrite(SPEAKER_LEDC_CHANNEL, 0);
        beepOn = false;
        beepCount++;

        if (beepCount < targetBeeps) {
            // Schedule next beep after pause (microseconds)
            timerWrite(beepTimer, 0);
            timerAlarmWrite(beepTimer, currentBeepPause * 1000, false);
            timerAlarmEnable(beepTimer);
        } else {
            // All beeps completed, stop timer
            timerAlarmDisable(beepTimer);
            beepCount = 0;
            targetBeeps = 0;
        }
    } else {
        // Turn on beep if we haven't reached target count
        if (beepCount < targetBeeps) {
            // Check volume - mute if volume is 0
            if (currentBeepVolume <= 0) {
                ledcWrite(SPEAKER_LEDC_CHANNEL, 0);
            } else {
                // Calculate duty cycle based on volume (0-100% -> 0%-100%)
                uint32_t maxDuty = (1 << SPEAKER_LEDC_TIMER_BIT) / 2; // 50% max duty cycle
                uint32_t volumeDuty = (maxDuty * currentBeepVolume) / 100;
                ledcWrite(SPEAKER_LEDC_CHANNEL, volumeDuty);
            }
            beepOn = true;

            // Schedule beep off after beep duration (microseconds)
            timerWrite(beepTimer, 0);
            timerAlarmWrite(beepTimer, currentBeepDuration * 1000, false);
            timerAlarmEnable(beepTimer);
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

    // Create hardware timer (timer 0, divider 80 = 1MHz = 1µs per tick)
    beepTimer = timerBegin(0, 80, true);

    // Attach ISR
    timerAttachInterrupt(beepTimer, &beepTimerISR, true);

    // Start the timer (it will run continuously, we control it with alarms)
    timerStart(beepTimer);

    Serial.println("Speaker initialized");
}

// Internal beep function that accepts volume directly (doesn't touch speaker_volume)
static void beep_internal(int numBeeps, uint32_t frequency, uint32_t duration, uint32_t pauseMs, int volume) {
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

    // Update timing parameters and use provided volume (NOT speaker_volume)
    currentBeepDuration = duration;
    currentBeepPause = pauseMs;
    currentBeepVolume = volume;  // Use the volume parameter directly

    // Stop any ongoing beep sequence
    timerAlarmDisable(beepTimer);
    ledcWrite(SPEAKER_LEDC_CHANNEL, 0);

    // Reset counters and start new sequence
    beepCount = 0;
    targetBeeps = numBeeps;
    beepOn = false;

    // Reset timer counter and start the beep sequence with 1ms delay
    timerWrite(beepTimer, 0);
    timerAlarmWrite(beepTimer, 1000, false);
    timerAlarmEnable(beepTimer);
}

// Public beep function - uses global speaker_volume setting
void beep(int numBeeps, uint32_t frequency, uint32_t duration, uint32_t pauseMs) {
    beep_internal(numBeeps, frequency, duration, pauseMs, speaker_volume);
}

// Helper function to play a tone with volume scaling
// Calculates effective volume as a percentage of speaker_volume WITHOUT modifying it
static void playTone(int numBeeps, uint32_t frequency, uint32_t duration, uint32_t pauseMs, int volumePercent) {
    // Calculate effective volume (speaker_volume * tone's volumePercent)
    // This does NOT modify speaker_volume, avoiding EEPROM writes
    int effectiveVolume = (speaker_volume * volumePercent) / 100;

    // Use internal beep function with calculated volume
    beep_internal(numBeeps, frequency, duration, pauseMs, effectiveVolume);
}

// Predefined tone functions
// parameters: beeps, freq, dur, pause, vol%
void tone_startup() {
    playTone(1, 2500, 40, 0, 70);  // Single short mid-high beep
}

void tone_sleep() {
    playTone(1, 2500, 500, 0, 50);  // Single long mid-high beep
}

void tone_message() {
    playTone(2, 2500, 100, 100, 80);  // Double beeps
}

void tone_alert() {
    playTone(1, 1500, 250, 100, 50);  // One long mid beep
}

void tone_urgent() {
    playTone(4, 1500, 200, 100, 80);  // Quad rapid mid beeps
}

void tone_confirm() {
    playTone(1, 2200, 50, 0, 60);  // Quick chirp
}

void tone_click() {
    playTone(1, 20, 51, 0, 40);  // Brief low freq click for haptic feedback
}

void tone_error() {
    playTone(1, 90, 150, 0, 10);  // Single low beep
}

