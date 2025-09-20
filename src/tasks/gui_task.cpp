#include "gui_task.h"
#include "globals.h"
#include <lvgl.h>
#include "ui/ui.h"

void guiTask(void *parameter) {
    while (true) {
        lv_tick_inc(millis() - lastTick);
        lastTick = millis();
        lv_timer_handler();
        ui_tick();
        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}