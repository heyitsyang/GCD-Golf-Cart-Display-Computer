#include "gui_task.h"
#include "globals.h"
#include "config.h"
#include <lvgl.h>
#include "ui_eez/ui.h"
#include "ui_eez/screens.h"
#include "ui_eez/styles.h"
#include "ui/venue_event_display.h"
#include "get_set_vars.h"

void updateEspnowIndicatorColor() {
    static bool last_espnow_connected_state = false;
    static bool initialized = false;

    // Only update if we're on the info screen that has the espnow_indicator
    lv_obj_t* current_screen = lv_scr_act();
    if (current_screen == nullptr || current_screen != objects.info) {
        // Reset initialization when not on info screen
        initialized = false;
        return;
    }

    // Check if espnow_indicator exists and is valid
    if (objects.espnow_indicator == nullptr) {
        return;
    }

    bool current_state = get_var_espnow_connected();

    // Initialize or update when state changes
    if (!initialized || current_state != last_espnow_connected_state) {
        if (current_state) {
            // Connected - apply green color (#00ff2d)
            lv_obj_set_style_text_color(objects.espnow_indicator, lv_color_hex(0xff00ff2d), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            // Disconnected - apply red color (#ff0000)
            lv_obj_set_style_text_color(objects.espnow_indicator, lv_color_hex(0xffff0000), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        last_espnow_connected_state = current_state;
        initialized = true;
    }
}

void updateEspnowGciMacColor() {
    static bool last_espnow_connected_state = false;
    static bool initialized = false;

    // Only update if we're on the settings2 screen that has obj5 (GCI MAC address label)
    lv_obj_t* current_screen = lv_scr_act();
    if (current_screen == nullptr || current_screen != objects.settings2) {
        // Reset initialization when not on settings2 screen
        initialized = false;
        return;
    }

    // Check if obj5 exists and is valid
    if (objects.obj5 == nullptr) {
        return;
    }

    bool current_state = get_var_espnow_connected();

    // Initialize or update when state changes
    if (!initialized || current_state != last_espnow_connected_state) {
        if (current_state) {
            // Connected - apply green color (#00ff2d)
            lv_obj_set_style_text_color(objects.obj5, lv_color_hex(0xff00ff2d), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            // Disconnected - apply red color (#ff0000)
            lv_obj_set_style_text_color(objects.obj5, lv_color_hex(0xffff0000), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        last_espnow_connected_state = current_state;
        initialized = true;
    }
}

void guiTask(void *parameter) {
    static uint32_t last_flag_set_time = 0;
    static uint32_t last_inactivity_check = 0;
    static lv_obj_t* previous_screen = nullptr;

    while (true) {
        uint32_t now = millis();

        lv_tick_inc(now - lastTick);
        lastTick = now;
        lv_timer_handler();
        ui_tick();

        // Update espnow indicator color based on connection state
        updateEspnowIndicatorColor();

        // Update espnow GCI MAC address color on Settings2 screen
        updateEspnowGciMacColor();

        // Check for screen changes and reset countdown if screen changed
        lv_obj_t* current_screen = lv_scr_act();
        if (current_screen != previous_screen) {
            // Check if we're leaving the Now Playing screen
            if (previous_screen == objects.now_playing && current_screen != objects.now_playing) {
                onNowPlayingScreenExit();
            }

            previous_screen = current_screen;
            // Reset countdown when entering a new screen (except splash)
            if (current_screen != objects.splash) {
                set_var_screen_inactivity_countdown(SCREEN_INACTIVITY_TIMEOUT_MS);
            }
        }

        // Check if Now Playing screen needs updating when new data flag is set
        if (new_rx_data_flag) {
            // Record when flag was set (if this is the first time we see it)
            if (last_flag_set_time == 0) {
                last_flag_set_time = now;
            }

            // Check for Now Playing screen updates
            checkAndUpdateNowPlayingScreen();

            // Auto-reset flag after configured time
            if ((now - last_flag_set_time) >= NEW_RX_DATA_FLAG_RESET_TIME) {
                new_rx_data_flag = false;
                last_flag_set_time = 0;
            }
        } else {
            // Reset timer when flag is not set
            last_flag_set_time = 0;
        }

        // Handle inactivity countdown (check every 100ms)
        if ((now - last_inactivity_check) >= 100) {
            handleInactivityCountdown(now);
            last_inactivity_check = now;
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void handleInactivityCountdown(uint32_t now) {
    lv_obj_t* current_screen = lv_scr_act();

    // Skip countdown on splash screen
    if (current_screen == objects.splash) {
        set_var_screen_inactivity_countdown(0);
        return;
    }

    // Decrement countdown if active
    if (get_var_screen_inactivity_countdown() > 0) {
        int32_t remaining = get_var_screen_inactivity_countdown() - 100;
        if (remaining <= 0) {
            remaining = 0;
        }
        set_var_screen_inactivity_countdown(remaining);
    }
}