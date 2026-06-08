#include <lvgl.h>
#include "ui_eez/screens.h"
#include "get_set_vars.h"
#include "globals.h"
#include "settings2_screen.h"
#include "communication/espnow_handler.h"
#include "storage/preferences_manager.h"
#include "config.h"

// Guard time before committing a cycle-button selection to NVM/ESP-NOW.
// Long enough that the user can cycle past intermediate options without
// triggering intermediate writes.
#define CYCLE_WRITE_DEBOUNCE_MS 3000

static const char * const FUEL_NAMES[] = {
    "NO FUEL SENSOR", "ADC GAS", "GPIO EXPANDER", "ADC ELECTRIC"
};

// millis() timestamp of last fuel-type tap; 0 = no write pending
static uint32_t s_fuelWritePendingMs = 0;

static void fuelCycleCb(lv_event_t *) {
    int32_t next = (fuelSensorType + 1) % 4;
    fuelSensorType = next;
    // GPIO_EXP requires minimum 25% low-fuel threshold
    if (next == FUEL_SENSOR_GPIO_EXP && fuel_low_percent < 25.0f)
        set_var_fuel_low_percent(25.0f);
    lv_label_set_text(objects.lbl_fuel_sensor_type, FUEL_NAMES[next]);
    s_fuelWritePendingMs = millis();
}

void settings2ScreenInit() {
    if (!objects.btn_fuel_sensor_type) return;
    lv_label_set_text(objects.lbl_fuel_sensor_type, FUEL_NAMES[fuelSensorType]);
    lv_obj_add_event_cb(objects.btn_fuel_sensor_type, fuelCycleCb, LV_EVENT_CLICKED, nullptr);
}

void settings2ScreenPump() {
    if (lv_scr_act() != objects.settings2) return;
    if (s_fuelWritePendingMs != 0 &&
        (millis() - s_fuelWritePendingMs) >= CYCLE_WRITE_DEBOUNCE_MS) {
        queuePreferenceWrite("fuel_sense_type", (int)fuelSensorType);
        espNow.sendFuelConfig();
        s_fuelWritePendingMs = 0;
    }
}

void settings2ScreenOnExit() {
    if (s_fuelWritePendingMs != 0) {
        queuePreferenceWrite("fuel_sense_type", (int)fuelSensorType);
        espNow.sendFuelConfig();
        s_fuelWritePendingMs = 0;
    }
}
