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

// True once the GPS row + home-loc section has been revealed this visit.
// Reset on exit so next entry always starts with the reduced widget set.
static bool s_gpsRowShown = false;

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

    // Hide GPS row and home-loc section until GPS has a position fix.
    // Each hidden widget contributes 0 draw tasks to the first-render pass,
    // reducing the peak from ~42KB to ~23KB and eliminating the race with
    // the WiFi pvPortMalloc(640) that caused the StoreProhibited crash.
    lv_obj_add_flag(objects.container_gps_hide, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.sats_hdop_1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.lbl_sats_hdop_value, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.lbl_cur_lat_value, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.lbl_cur_long_value, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.at_home_indicator, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.btn_set_home_loc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.set_home_loc_value, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.home_gps_fence_radius, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.lbl_home_gps_fence_radius_m, LV_OBJ_FLAG_HIDDEN);
    s_gpsRowShown = false;
}

void settings2ScreenPump() {
    if (lv_scr_act() != objects.settings2) return;

    // Reveal GPS row and home-loc section once GPS has a position fix.
    // Un-hiding triggers incremental redraw of only the newly visible area — no heap spike.
    if (!s_gpsRowShown && fix.valid.location) {
        lv_obj_clear_flag(objects.container_gps_hide, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.sats_hdop_1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.lbl_sats_hdop_value, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.lbl_cur_lat_value, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.lbl_cur_long_value, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.at_home_indicator, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.btn_set_home_loc, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.set_home_loc_value, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.home_gps_fence_radius, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.lbl_home_gps_fence_radius_m, LV_OBJ_FLAG_HIDDEN);
        s_gpsRowShown = true;
    }

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
    // Re-hide so next entry starts with the reduced widget set.
    // NULL-guard because exit hook may run before screen is fully initialized.
    if (objects.container_gps_hide)
        lv_obj_add_flag(objects.container_gps_hide, LV_OBJ_FLAG_HIDDEN);
    if (objects.sats_hdop_1)
        lv_obj_add_flag(objects.sats_hdop_1, LV_OBJ_FLAG_HIDDEN);
    if (objects.lbl_sats_hdop_value)
        lv_obj_add_flag(objects.lbl_sats_hdop_value, LV_OBJ_FLAG_HIDDEN);
    if (objects.lbl_cur_lat_value)
        lv_obj_add_flag(objects.lbl_cur_lat_value, LV_OBJ_FLAG_HIDDEN);
    if (objects.lbl_cur_long_value)
        lv_obj_add_flag(objects.lbl_cur_long_value, LV_OBJ_FLAG_HIDDEN);
    if (objects.at_home_indicator)
        lv_obj_add_flag(objects.at_home_indicator, LV_OBJ_FLAG_HIDDEN);
    if (objects.btn_set_home_loc)
        lv_obj_add_flag(objects.btn_set_home_loc, LV_OBJ_FLAG_HIDDEN);
    if (objects.set_home_loc_value)
        lv_obj_add_flag(objects.set_home_loc_value, LV_OBJ_FLAG_HIDDEN);
    if (objects.home_gps_fence_radius)
        lv_obj_add_flag(objects.home_gps_fence_radius, LV_OBJ_FLAG_HIDDEN);
    if (objects.lbl_home_gps_fence_radius_m)
        lv_obj_add_flag(objects.lbl_home_gps_fence_radius_m, LV_OBJ_FLAG_HIDDEN);
    s_gpsRowShown = false;
}
