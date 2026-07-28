#include <lvgl.h>
#include "ui_eez/screens.h"
#include "globals.h"
#include "settings_screen.h"

// The GPS readout row (sats/HDOP, lat, long) lives on the Settings screen.
// It is hidden until GPS reports a valid fix, so the user never sees an empty
// or stale position. Mirrors the Settings2 pattern in settings2_screen.cpp.
//
// These widgets used to live on Settings2 and were driven by that screen's
// hide/reveal. When they moved here, the Settings2 logic kept hiding them but
// could never reveal them (its pump returns early unless Settings2 is active),
// so they stayed invisible on Settings. Worse, toggling HIDDEN on them while
// Settings2 was the active screen invalidated objects on an inactive screen —
// the sluggish-transition trap documented in settings2ScreenOnExit().

// True once the GPS row has been revealed this visit.
// Reset by settingsLoadStartCb so every entry starts hidden.
static bool s_gpsRowShown = false;

static void doHideGpsWidgets() {
    lv_obj_add_flag(objects.sats_hdop_1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.lbl_sats_hdop_value, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.lbl_cur_lat_value, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.lbl_cur_long_value, LV_OBJ_FLAG_HIDDEN);
    s_gpsRowShown = false;
}

// Fires on LV_EVENT_SCREEN_LOAD_START — before the first animation frame of each
// Settings visit. Hiding here runs on the loading (active) screen, which is a
// safe LVGL call site.
static void settingsLoadStartCb(lv_event_t *) {
    doHideGpsWidgets();
}

void settingsScreenInit() {
    if (!objects.settings || !objects.sats_hdop_1) return;

    lv_obj_add_event_cb(objects.settings, settingsLoadStartCb, LV_EVENT_SCREEN_LOAD_START, nullptr);

    // Initial hide at boot: Settings is pre-created but not yet loaded, so the
    // callback above has not fired for the first visit.
    doHideGpsWidgets();
}

void settingsScreenPump() {
    if (lv_scr_act() != objects.settings) return;

    // Reveal once GPS has a position fix. Un-hiding redraws only the newly
    // visible area — no heap spike.
    if (!s_gpsRowShown && fix.valid.location) {
        lv_obj_clear_flag(objects.sats_hdop_1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.lbl_sats_hdop_value, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.lbl_cur_lat_value, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.lbl_cur_long_value, LV_OBJ_FLAG_HIDDEN);
        s_gpsRowShown = true;
    }
}
