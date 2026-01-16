#include "system_task.h"
#include "config.h"
#include "globals.h"
#include "types.h"
#include "hardware/display.h"
#include "storage/preferences_manager.h"
#include "utils/sleep_manager.h"
#include "communication/meshtastic_admin.h"
#include "tasks/meshtastic_task.h"

// Helper function to check if debounce period has elapsed
static bool debounceElapsed(uint32_t debounce_time) {
    if (debounce_time == 0) return false;  // No pending write
    return (millis() - debounce_time) >= EEPROM_DEBOUNCE_MS;
}

void systemTask(void *parameter) {
    while (true) {
        uint32_t now = millis();

        // Initialize GPS config after Meshtastic connection (polled approach to avoid stack overflow in callback)
        initGpsConfigOnBoot();

        // Capture GCM node ID after Meshtastic connection is established
        requestMetadataOnce();

        // Process sleep mode state machine (highest priority - check first)
        if (processSleepModeStateMachine()) {
            enterDeepSleep();
            // Device will reboot from setup() on wake - code never returns here
        }

        // Handle preferences reset
        if (reset_preferences == true) {
            reset_preferences = false;  // Reset flag before clearing to prevent restart loop
            clearAllPreferences();
        }

        // Handle manual reboot
        if (manual_reboot == true) {
            ESP.restart();
        }

        // === DEBOUNCED EEPROM WRITES ===
        // For UI-adjustable values (sliders, spinners), we delay EEPROM writes
        // until the value has been stable for EEPROM_DEBOUNCE_MS to reduce wear.
        // Immediate effects (like backlight changes) still happen right away.

        // Day backlight - immediate visual feedback, debounced EEPROM write
        if (day_backlight != old_day_backlight) {
            setBacklight((day_backlight * 20) + 55);
            old_day_backlight = day_backlight;
            debounce_day_backlight = now;  // Start/reset debounce timer
        }
        if (debounceElapsed(debounce_day_backlight)) {
            queuePreferenceWrite("day_backlight", day_backlight);
            debounce_day_backlight = 0;  // Clear pending write
        }

        // Night backlight - immediate visual feedback, debounced EEPROM write
        if (night_backlight != old_night_backlight) {
            setBacklight(night_backlight * 20);
            old_night_backlight = night_backlight;
            debounce_night_backlight = now;
        }
        if (debounceElapsed(debounce_night_backlight)) {
            queuePreferenceWrite("night_backlight", night_backlight);
            debounce_night_backlight = 0;
        }

        // GCI MAC address - not a slider, write immediately
        if (espnow_gci_mac_addr != old_espnow_gci_mac_addr) {
            queuePreferenceWrite("gci_mac", espnow_gci_mac_addr);
            old_espnow_gci_mac_addr = espnow_gci_mac_addr;
        }

        // Speaker volume - debounced
        if (speaker_volume != old_speaker_volume) {
            old_speaker_volume = speaker_volume;
            debounce_speaker_volume = now;
        }
        if (debounceElapsed(debounce_speaker_volume)) {
            queuePreferenceWrite("speaker_volume", speaker_volume);
            debounce_speaker_volume = 0;
        }

        // Service interval hours - debounced
        if (svc_interval_hrs != old_svc_interval_hrs) {
            old_svc_interval_hrs = svc_interval_hrs;
            debounce_svc_interval_hrs = now;
        }
        if (debounceElapsed(debounce_svc_interval_hrs)) {
            queuePreferenceWrite("svc_interval_hrs", svc_interval_hrs);
            debounce_svc_interval_hrs = 0;
        }

        // Temperature adjustment - debounced
        if (temperature_adj != old_temperature_adj) {
            old_temperature_adj = temperature_adj;
            debounce_temperature_adj = now;
        }
        if (debounceElapsed(debounce_temperature_adj)) {
            queuePreferenceWrite("temperature_adj", temperature_adj);
            debounce_temperature_adj = 0;
        }

        // Backlight timeout - debounced
        if (backlight_timeout != old_backlight_timeout) {
            old_backlight_timeout = backlight_timeout;
            debounce_backlight_timeout = now;
        }
        if (debounceElapsed(debounce_backlight_timeout)) {
            queuePreferenceWrite("bklt_timeout", backlight_timeout);
            debounce_backlight_timeout = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(100));  // Check sleep pin frequently (was 1000ms)
    }
}