#include <lvgl.h>
#include "ui_eez/screens.h"
#include "get_set_vars.h"
#include "globals.h"
#include "settings2_screen.h"

static const char * const FUEL_NAMES[] = {
    "NO FUEL SENSOR", "ADC GAS", "GPIO EXPANDER", "ADC ELECTRIC"
};

static void fuelCycleCb(lv_event_t *) {
    int32_t next = (fuelSensorType + 1) % 4;
    set_var_fuel_sense_type(next);
    lv_label_set_text(objects.lbl_fuel_sensor_type, FUEL_NAMES[next]);
}

void settings2ScreenInit() {
    if (!objects.btn_fuel_sensor_type) return;
    lv_label_set_text(objects.lbl_fuel_sensor_type, FUEL_NAMES[fuelSensorType]);
    lv_obj_add_event_cb(objects.btn_fuel_sensor_type, fuelCycleCb, LV_EVENT_CLICKED, nullptr);
}
