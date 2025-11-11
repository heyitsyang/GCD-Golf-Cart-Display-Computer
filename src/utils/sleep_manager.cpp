#include "sleep_manager.h"
#include "config.h"
#include "globals.h"
#include "hardware/display.h"
#include "get_set_vars.h"
#include <esp_sleep.h>
#include <driver/rtc_io.h>

void initSleepPin() {
    // Configure SLEEP_PIN as INPUT with internal pullup
    // Default HIGH = awake, LOW = sleep
    pinMode(SLEEP_PIN, INPUT_PULLUP);

    Serial.print("Sleep pin (GPIO ");
    Serial.print(SLEEP_PIN);
    Serial.println(") initialized as INPUT_PULLUP");
    Serial.println("Device will enter deep sleep when pin is LOW, reboot when HIGH");
}

bool shouldEnterSleep() {
    // Return true if SLEEP_PIN is LOW (pulled to ground)
    return digitalRead(SLEEP_PIN) == LOW;
}

void enterDeepSleep() {
    Serial.println("SLEEP_PIN is LOW - entering deep sleep mode...");

    // Turn off backlight to save power
    setBacklight(0);

    // Turn off speaker
    ledcWrite(SPEAKER_LEDC_CHANNEL, 0);

    // Configure SLEEP_PIN as EXT0 wake source
    // Wake when pin goes HIGH (1)
    // ESP32 will reboot from setup() when woken
    esp_sleep_enable_ext0_wakeup((gpio_num_t)SLEEP_PIN, 1);

    Serial.println("Entering deep sleep (will reboot on SLEEP_PIN HIGH)...");
    Serial.println("Maximum power savings: ~10uA");
    Serial.flush(); // Ensure message is sent before sleep

    // Enter deep sleep mode
    // This turns off everything except RTC and wake sources
    // Device will restart from setup() when SLEEP_PIN goes HIGH
    esp_deep_sleep_start();

    // ===== CODE NEVER REACHES HERE =====
    // Device reboots from setup() on wake
}
