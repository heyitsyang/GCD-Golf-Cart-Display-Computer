#include "gui_task.h"
#include "globals.h"
#include "config.h"
#include <lvgl.h>
#include "ui_eez/ui.h"
#include "ui_eez/screens.h"
#include "ui/venue_event_display.h"
#include "get_set_vars.h"

void guiTask(void *parameter) {
    static uint32_t last_flag_set_time = 0;
    static uint32_t last_inactivity_check = 0;

    while (true) {
        uint32_t now = millis();

        lv_tick_inc(now - lastTick);
        lastTick = now;
        lv_timer_handler();
        ui_tick();

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
                Serial.println("GUI: Auto-reset new_rx_data_flag");
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

    // Initialize countdown if not set and we have touch activity
    if (get_var_screen_inactivity_countdown() == 0 && lastTouchActivity > 0) {
        set_var_screen_inactivity_countdown(SCREEN_INACTIVITY_TIMEOUT_MS);
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