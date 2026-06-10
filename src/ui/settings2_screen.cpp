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
// Reset by settings2LoadStartCb so next entry always starts with the reduced widget set.
static bool s_gpsRowShown = false;

// Hides GPS row and home-loc section. Called at boot and via SCREEN_LOAD_START so
// widgets are hidden before the first render of every Settings2 visit, keeping peak
// draw-task heap low and preventing the OOM that plagued the original render pass.
static void doHideGpsWidgets() {
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

// Fires on LV_EVENT_SCREEN_LOAD_START — before the first animation frame of each
// Settings2 visit. Hiding here (on the loading screen, not an inactive screen)
// keeps the call legal and ensures widgets are hidden before any draw pass.
static void settings2LoadStartCb(lv_event_t *) {
    doHideGpsWidgets();
}

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

    // Register pre-render hide for every future visit to Settings2.
    // SCREEN_LOAD_START fires before the first animation frame, so the hide
    // runs on the loading (active) screen — a safe LVGL call site.
    lv_obj_add_event_cb(objects.settings2, settings2LoadStartCb, LV_EVENT_SCREEN_LOAD_START, nullptr);

    // Initial hide at boot: Settings2 is pre-created but not yet loaded.
    // doHideGpsWidgets() here covers the first visit before the callback fires.
    doHideGpsWidgets();
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
    // No LVGL calls here — settings2LoadStartCb re-hides GPS widgets before
    // the next Settings2 render. See memory note "lvgl-offscreen-invalidate":
    // lv_obj_add_flag on inactive-screen objects queues dirty regions and
    // corrupts the Home screen refresh that follows this exit.
}
