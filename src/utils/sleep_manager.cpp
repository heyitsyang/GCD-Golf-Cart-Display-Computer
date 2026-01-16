#include "sleep_manager.h"
#include "config.h"
#include "globals.h"
#include "hardware/display.h"
#include "get_set_vars.h"
#include "communication/meshtastic_admin.h"
#include "storage/preferences_manager.h"
#include "Meshtastic.h"
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
    queuePreferenceWrite("accumDistance", accum_distance);
    queuePreferenceWrite("tripDistance", trip_distance);
    queuePreferenceWrite("hrs_since_svc", hrs_since_svc);  // Saved as tenths of hours
    delay(150);  // Give EEPROM task time to process the queue

    // Cleanly shutdown Meshtastic serial connection
    if (mesh_serial_enabled) {
        // Reset GPS update interval to default (2 minutes) to reduce radio power consumption
        // Must be done while serial is still active
        resetGpsIntervalBeforeSleep();

        Serial.println("Shutting down Meshtastic serial connection...");
        mt_serial_end();  // Directly shutdown UART2 before sleep
        mesh_serial_enabled = false;  // Update state to match
    }

    // Play sleep tone
    tone_sleep();
    delay(550);  // Wait for tone to complete (500ms duration + 50ms buffer)

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

/**
 * Initialize the sleep mode state machine
 */
void initSleepModeStateMachine() {
    startup_time_ms = millis();
    sleep_operating_mode = SLEEP_MODE_STARTUP_GRACE;
    gci_communicated_flag = false;
    backlight_dimmed = false;
    last_activity_time_ms = millis();
    gci_disconnect_time_ms = 0;  // 0 = GCI connected (or never was)

    Serial.println("Sleep mode state machine initialized - STARTUP_GRACE period");
    Serial.print("Grace period: ");
    Serial.print(backlight_timeout);
    Serial.println(" minutes");
}

/**
 * Check for activity (touch or movement) and update last_activity_time_ms
 */
static void checkActivityForBacklight() {
    bool activity_detected = false;

    // Check for touch activity (compare with stored last touch time)
    if (lastTouchActivity > last_activity_time_ms) {
        activity_detected = true;
    }

    // Check for movement (speed > 0)
    if (avg_speed > 0) {
        activity_detected = true;
    }

    if (activity_detected) {
        last_activity_time_ms = millis();
    }
}

/**
 * Handle backlight dimming in NO_GCI_MODE
 * Backlight dims when BOTH touch AND movement have been inactive for backlight_timeout minutes
 */
static void handleNoGciBacklight() {
    // Only applicable in NO_GCI_MODE
    if (sleep_operating_mode != SLEEP_MODE_NO_GCI) {
        return;
    }

    uint32_t now = millis();
    uint32_t inactivity_duration_ms = now - last_activity_time_ms;
    uint32_t timeout_ms = (uint32_t)backlight_timeout * 60UL * 1000UL;

    if (inactivity_duration_ms >= timeout_ms && !backlight_dimmed) {
        // Turn off backlight
        setBacklight(0);
        backlight_dimmed = true;
        Serial.println("NO_GCI_MODE: Backlight dimmed due to inactivity");
    } else if (inactivity_duration_ms < timeout_ms && backlight_dimmed) {
        // Restore backlight based on day_backlight setting
        setBacklight((day_backlight * 20) + 55);
        backlight_dimmed = false;
        Serial.println("NO_GCI_MODE: Backlight restored due to activity");
    }
}

/**
 * Check if GCI is paired and actively communicating
 * Returns true if GCI MAC is set AND espnow_connected is true
 */
static bool isGciActivelyConnected() {
    // Check if GCI is paired (MAC saved and not "NONE")
    if (espnow_gci_mac_addr == "NONE" || espnow_gci_mac_addr.length() != 17) {
        return false;
    }
    // Check if actively communicating (heartbeat within timeout)
    return espnow_connected;
}

/**
 * Process the sleep mode state machine
 * Returns true if deep sleep should be entered
 */
bool processSleepModeStateMachine() {
#ifdef DEMO_MODE
    // Demo mode: never enter sleep
    return false;
#endif

    uint32_t now = millis();
    uint32_t timeout_ms = (uint32_t)backlight_timeout * 60UL * 1000UL;

    // Update activity tracking for backlight dimming
    checkActivityForBacklight();

    switch (sleep_operating_mode) {
        case SLEEP_MODE_STARTUP_GRACE:
            // Check if GCI has communicated
            if (isGciActivelyConnected()) {
                gci_communicated_flag = true;
                sleep_operating_mode = SLEEP_MODE_GCI;
                gci_disconnect_time_ms = 0;  // Reset disconnect timer
                Serial.println("*** Transitioning to GCI_MODE - GCI connection established ***");
            }
            // Check if grace period expired
            else if ((now - startup_time_ms) >= timeout_ms) {
                if (gci_communicated_flag) {
                    sleep_operating_mode = SLEEP_MODE_GCI;
                    gci_disconnect_time_ms = 0;
                    Serial.println("*** Transitioning to GCI_MODE - GCI was connected during grace period ***");
                } else {
                    sleep_operating_mode = SLEEP_MODE_NO_GCI;
                    Serial.println("*** Transitioning to NO_GCI_MODE - No GCI communication during grace period ***");
                }
            }
            // During grace period, never sleep regardless of SLEEP_PIN state
            return false;

        case SLEEP_MODE_GCI:
            // Check if GCI is still connected
            if (isGciActivelyConnected()) {
                // GCI is connected - reset disconnect timer
                gci_disconnect_time_ms = 0;
            } else {
                // GCI disconnected - start or continue disconnect timer
                if (gci_disconnect_time_ms == 0) {
                    gci_disconnect_time_ms = now;
                    Serial.println("GCI_MODE: GCI disconnected, starting timeout timer");
                } else if ((now - gci_disconnect_time_ms) >= timeout_ms) {
                    // GCI has been disconnected for backlight_timeout minutes
                    sleep_operating_mode = SLEEP_MODE_NO_GCI;
                    Serial.println("*** Transitioning to NO_GCI_MODE - GCI disconnected for timeout period ***");
                    return false;  // Don't sleep on transition, will handle in NO_GCI_MODE
                }
            }

            // In GCI mode, sleep is allowed when SLEEP_PIN is LOW
            if (shouldEnterSleep()) {
                return true;  // Allow deep sleep
            }
            return false;

        case SLEEP_MODE_NO_GCI:
            // Check if GCI reconnects (paired and heartbeat detected)
            if (isGciActivelyConnected()) {
                sleep_operating_mode = SLEEP_MODE_GCI;
                gci_disconnect_time_ms = 0;
                // Restore backlight if it was dimmed
                if (backlight_dimmed) {
                    setBacklight((day_backlight * 20) + 55);
                    backlight_dimmed = false;
                }
                Serial.println("*** Transitioning to GCI_MODE - GCI reconnected ***");
                return false;
            }

            // In NO_GCI mode, never enter deep sleep
            // Handle backlight dimming instead
            handleNoGciBacklight();
            return false;
    }

    return false;
}
