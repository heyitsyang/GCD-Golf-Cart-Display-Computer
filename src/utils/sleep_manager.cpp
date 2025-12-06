#include "sleep_manager.h"
#include "config.h"
#include "globals.h"
#include "hardware/display.h"
#include "get_set_vars.h"
#include "communication/meshtastic_admin.h"
#include "storage/preferences_manager.h"
#include <esp_sleep.h>
#include <driver/rtc_io.h>

void initSleepPin() {
    // Configure SLEEP_PIN as INPUT with internal pullup
    // Default HIGH = awake, LOW = sleep
    pinMode(SLEEP_PIN, INPUT_PULLUP);

    Serial.print("Sleep pin (GPIO ");
    Serial.print(SLEEP_PIN);
    Serial.println(") initialized as INPUT_PULLUP");
#ifdef DEMO_MODE
    Serial.println("*** DEMO MODE: Sleep functionality DISABLED ***");
#else
    Serial.println("Device will enter deep sleep when pin is LOW, reboot when HIGH");
#endif
}

bool shouldEnterSleep() {
#ifdef DEMO_MODE
    // Demo mode: never enter sleep, ignore SLEEP_PIN
    return false;
#else
    // Return true if SLEEP_PIN is LOW (pulled to ground)
    return digitalRead(SLEEP_PIN) == LOW;
#endif
}

void enterDeepSleep() {
    Serial.println("SLEEP_PIN is LOW - entering deep sleep mode...");

    // Save distance and hours values to EEPROM before sleeping
    Serial.println("Saving distance and hours to EEPROM...");
    queuePreferenceWrite("accumDistance", accumDistance);
    queuePreferenceWrite("tripDistance", tripDistance);
    queuePreferenceWrite("hrs_since_svc", hrs_since_svc);  // Saved as tenths of hours
    delay(150);  // Give EEPROM task time to process the queue

    // Reset GPS update interval to default (2 minutes) to reduce radio power consumption
    resetGpsIntervalBeforeSleep();

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
