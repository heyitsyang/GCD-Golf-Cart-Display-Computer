#include "gui_task.h"
#include "globals.h"
#include "config.h"
#include <lvgl.h>
#include "ui/ui.h"
#include "ui/venue_event_display.h"

void guiTask(void *parameter) {
    static uint32_t last_flag_set_time = 0;

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

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}