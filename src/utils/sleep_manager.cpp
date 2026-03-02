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

// Track current GPS interval setting to avoid redundant commands
// -1 = unknown/not set, 0 = default (2 min), 8 = fast (8 sec)
static int8_t current_gps_interval = -1;

// Minimum startup grace period in seconds (used when backlight_timeout = 0)
// Ensures GCI has time to send heartbeat before transitioning to NO_GCI_MODE
static const uint32_t MIN_STARTUP_GRACE_SECS = 30;

// SLEEP_PIN debounce configuration
// Requires pin to be stable for this duration before acting on state change
static const uint32_t SLEEP_PIN_DEBOUNCE_MS = 400;

// Debounce state tracking
static int last_sleep_pin_state = HIGH;      // Last stable state (HIGH = awake)
static int current_raw_state = HIGH;         // Current raw reading
static uint32_t state_change_time_ms = 0;    // When the state last changed
static bool debounced_sleep_state = false;   // Debounced result: true = should sleep

/**
 * Set GCM GPS update interval
 * interval: 0 = default (2 min), 8 = fast (8 sec)
 * Only sends command if interval has changed
 */
static void setGpsInterval(int8_t interval) {
    if (current_gps_interval == interval) return;
    if (not_yet_connected || !mesh_serial_enabled) return;

    meshtastic_Config_PositionConfig config;
    if (!getRadioPositionConfig(&config)) return;  // Config not yet captured
    config.gps_update_interval = interval;

    if (mt_set_position_config(&config)) {
        current_gps_interval = interval;
#if DEBUG_SLEEP_STATE == 1
        Serial.printf("NO_GCI_MODE: GPS interval set to %s\n",
                      interval == 0 ? "default (2 min) - at home" : "8 sec - away from home");
#endif
    }
}

void initSleepPin() {
    // Configure SLEEP_PIN as INPUT (GPIO 35 has no internal pull-up/down)
    // External pull-down used: LOW = sleep (ignition OFF), HIGH = awake (ignition ON)
    pinMode(SLEEP_PIN, INPUT);

    // Initialize debounce state to current pin reading
    last_sleep_pin_state = digitalRead(SLEEP_PIN);
    current_raw_state = last_sleep_pin_state;
    state_change_time_ms = millis();
    debounced_sleep_state = (last_sleep_pin_state == LOW);

#if DEBUG_INIT == 1
    Serial.printf("Sleep pin (GPIO %d) initialized - state: %s, debounce: %dms\n",
                  SLEEP_PIN, last_sleep_pin_state == HIGH ? "HIGH" : "LOW", SLEEP_PIN_DEBOUNCE_MS);
#endif
}

/**
 * Reset debounce state when transitioning to GCI_MODE
 * Sets safe default (don't sleep) and lets normal debounce logic determine actual state
 * By assuming previous state was HIGH, if pin is actually LOW the debounce logic
 * will detect the "change" and trigger sleep after the debounce period
 */
static void resetSleepPinDebounce() {
    int pin_reading = digitalRead(SLEEP_PIN);
    // Assume previous state was HIGH (awake) so debounce logic can detect LOW
    last_sleep_pin_state = HIGH;
    current_raw_state = pin_reading;
    state_change_time_ms = millis();
    // Default to NOT sleeping - debounce logic will set true if pin stays LOW
    debounced_sleep_state = false;

#if DEBUG_SLEEP_STATE == 1
    Serial.printf("SLEEP_PIN debounce reset: pin=%s, will sleep after %dms if LOW\n",
                  pin_reading == HIGH ? "HIGH" : "LOW", SLEEP_PIN_DEBOUNCE_MS);
#endif
}

bool shouldEnterSleep() {
    // Debounced sleep pin reading
    // Requires pin to be stable in new state for SLEEP_PIN_DEBOUNCE_MS
    // This prevents rapid transitions (e.g., LOW-HIGH-LOW) from causing incorrect state

    int raw_reading = digitalRead(SLEEP_PIN);
    uint32_t now = millis();

    // If raw reading changed from what we were tracking
    if (raw_reading != current_raw_state) {
        current_raw_state = raw_reading;
        state_change_time_ms = now;
#if DEBUG_SLEEP_STATE == 1
        Serial.printf("SLEEP_PIN raw: %s (debouncing...)\n", raw_reading == HIGH ? "HIGH" : "LOW");
#endif
        // Don't change debounced state yet - wait for stability
    }

    // If current raw state has been stable for debounce period
    // and differs from last stable state, update the debounced state
    if (current_raw_state != last_sleep_pin_state) {
        if ((now - state_change_time_ms) >= SLEEP_PIN_DEBOUNCE_MS) {
            last_sleep_pin_state = current_raw_state;
            debounced_sleep_state = (current_raw_state == LOW);

#if DEBUG_SLEEP_STATE == 1
            Serial.printf("SLEEP_PIN debounced: %s\n", debounced_sleep_state ? "SLEEP" : "AWAKE");
#endif
        }
    }

    return debounced_sleep_state;
}

void enterDeepSleep() {
    Serial.println("Entering deep sleep...");

    // Save distance and hours values to EEPROM before sleeping
    queuePreferenceWrite("accumDistance", accum_distance);
    queuePreferenceWrite("tripDistance", trip_distance);
    queuePreferenceWrite("hrs_since_svc", hrs_since_svc);  // Saved as tenths of hours
    delay(150);  // Give EEPROM task time to process the queue

    // Cleanly shutdown Meshtastic serial connection
    if (mesh_serial_enabled) {
        // Reset GPS update interval to default (2 minutes) to reduce radio power consumption
        // Must be done while serial is still active
        resetGpsIntervalBeforeSleep();
        mt_serial_end();  // flush TX + close UART2
        mesh_serial_enabled = false;  // Update state to match
    }

    // Turn off backlight to save power
    setBacklight(0);

    // Turn off speaker
    ledcWrite(SPEAKER_LEDC_CHANNEL, 0);

    // Configure SLEEP_PIN as EXT0 wake source
    // Wake when pin goes HIGH (1)
    // ESP32 will reboot from setup() when woken
    esp_sleep_enable_ext0_wakeup((gpio_num_t)SLEEP_PIN, 1);

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

#if DEBUG_SLEEP_STATE == 1
    Serial.printf("Sleep state machine: STARTUP_GRACE (%d %s)\n",
                  backlight_timeout == 0 ? MIN_STARTUP_GRACE_SECS : backlight_timeout,
                  backlight_timeout == 0 ? "sec" : "min");
#endif
}

// Track last known touch timestamp to detect NEW touches
static uint32_t prev_lastTouchActivity = 0;

/**
 * Check for activity (touch or movement) and update last_activity_time_ms
 * Only logs when backlight is dimmed to help debug false wakeups
 */
static void checkActivityForBacklight() {
    bool touch_activity = false;
    bool movement_activity = false;

    // Check for NEW touch activity (lastTouchActivity changed since last check)
    if (lastTouchActivity != prev_lastTouchActivity) {
        prev_lastTouchActivity = lastTouchActivity;
        touch_activity = true;
    }

    // Check for movement (speed > 0)
    // Note: avg_speed (int32_t) should already be filtered by MIN_SPEED_FILTER_MPH in gps_task
    if (avg_speed > 0) {
        movement_activity = true;
    }

    if (touch_activity || movement_activity) {
#if DEBUG_SLEEP_STATE == 1
        // Debug: Log what triggered activity when backlight was dimmed
        if (backlight_dimmed) {
            Serial.printf("NO_GCI_MODE: Activity - %s%s\n",
                          touch_activity ? "TOUCH " : "",
                          movement_activity ? "MOVEMENT" : "");
        }
#endif
        last_activity_time_ms = millis();
    }
}

/**
 * Handle backlight dimming in NO_GCI_MODE
 * Backlight dims when BOTH touch AND movement have been inactive for backlight_timeout minutes
 * If backlight_timeout == 0, dimming is disabled
 */
static void handleNoGciBacklight() {
    // Only applicable in NO_GCI_MODE
    if (sleep_operating_mode != SLEEP_MODE_NO_GCI) {
        return;
    }

    // If backlight_timeout is 0, dimming is disabled
    if (backlight_timeout == 0) {
        if (backlight_dimmed) {
            // Restore backlight if it was previously dimmed
            setBacklight((day_backlight * 20) + 55);
            backlight_dimmed = false;
#if DEBUG_SLEEP_STATE == 1
            Serial.println("NO_GCI_MODE: Backlight restored - timeout disabled");
#endif
        }
        return;
    }

    uint32_t now = millis();
    uint32_t inactivity_duration_ms = now - last_activity_time_ms;
    uint32_t timeout_ms = (uint32_t)backlight_timeout * 60UL * 1000UL;

    if (inactivity_duration_ms >= timeout_ms && !backlight_dimmed) {
        // Turn off backlight
        setBacklight(0);
        backlight_dimmed = true;
#if DEBUG_SLEEP_STATE == 1
        Serial.println("NO_GCI_MODE: Backlight dimmed");
#endif
    } else if (inactivity_duration_ms < timeout_ms && backlight_dimmed) {
        // Restore backlight based on day_backlight setting
        setBacklight((day_backlight * 20) + 55);
        backlight_dimmed = false;
#if DEBUG_SLEEP_STATE == 1
        Serial.println("NO_GCI_MODE: Backlight restored");
#endif
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
                resetSleepPinDebounce();     // Re-sample pin now that GCI is confirmed
                Serial.println("-> GCI_MODE (connected)");
            }
            // Check if grace period expired
            // Use minimum grace period if backlight_timeout is 0 to allow GCI time to connect
            else {
                uint32_t grace_timeout_ms = timeout_ms;
                if (backlight_timeout == 0) {
                    grace_timeout_ms = MIN_STARTUP_GRACE_SECS * 1000UL;
                }
                if ((now - startup_time_ms) >= grace_timeout_ms) {
                    if (gci_communicated_flag) {
                        sleep_operating_mode = SLEEP_MODE_GCI;
                        gci_disconnect_time_ms = 0;
                        resetSleepPinDebounce();  // Re-sample pin now that GCI is confirmed
                        Serial.println("-> GCI_MODE");
                    } else {
                        sleep_operating_mode = SLEEP_MODE_NO_GCI;
                        Serial.println("-> NO_GCI_MODE");
                    }
                }
            }
            // During grace period, never sleep regardless of SLEEP_PIN state
            return false;

        case SLEEP_MODE_GCI: {
            // Check if GCI is still connected
            if (isGciActivelyConnected()) {
                // GCI is connected - reset disconnect timer
                gci_disconnect_time_ms = 0;
            } else {
                // GCI disconnected - start or continue disconnect timer
                // Use minimum timeout if backlight_timeout is 0
                uint32_t disconnect_timeout_ms = timeout_ms;
                if (backlight_timeout == 0) {
                    disconnect_timeout_ms = MIN_STARTUP_GRACE_SECS * 1000UL;
                }
                if (gci_disconnect_time_ms == 0) {
                    gci_disconnect_time_ms = now;
#if DEBUG_SLEEP_STATE == 1
                    Serial.println("GCI_MODE: GCI disconnected, starting timeout");
#endif
                } else if ((now - gci_disconnect_time_ms) >= disconnect_timeout_ms) {
                    // GCI has been disconnected for timeout period
                    sleep_operating_mode = SLEEP_MODE_NO_GCI;
                    Serial.println("-> NO_GCI_MODE (timeout)");
                    return false;  // Don't sleep on transition, will handle in NO_GCI_MODE
                }
            }

            // In GCI mode, sleep is allowed when SLEEP_PIN is LOW
            if (shouldEnterSleep()) {
                return true;  // Allow deep sleep
            }
            return false;
        }

        case SLEEP_MODE_NO_GCI:
            // Check if GCI reconnects (paired and heartbeat detected)
            if (isGciActivelyConnected()) {
                sleep_operating_mode = SLEEP_MODE_GCI;
                gci_disconnect_time_ms = 0;
                resetSleepPinDebounce();  // Re-sample pin now that GCI is confirmed
                // Restore backlight if it was dimmed
                if (backlight_dimmed) {
                    setBacklight((day_backlight * 20) + 55);
                    backlight_dimmed = false;
                }
                // Reset GPS interval tracking - GCI_MODE uses SLEEP_PIN for interval control
                current_gps_interval = -1;
                Serial.println("-> GCI_MODE (reconnected)");
                return false;
            }

            // In NO_GCI mode, never enter deep sleep
            // Handle backlight dimming instead
            handleNoGciBacklight();

            // Manage GCM GPS update interval based on at_home status
            // At home: slow interval (0 = default 2 min) to save power
            // Away: fast interval (8 sec) for better tracking
            if (at_home) {
                setGpsInterval(0);   // Default 2 min interval
            } else {
                setGpsInterval(8);   // Fast 8 sec interval
            }

            return false;
    }

    return false;
}
