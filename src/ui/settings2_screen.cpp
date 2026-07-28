#include <lvgl.h>
#include "ui_eez/screens.h"
#include "get_set_vars.h"
#include "globals.h"
#include "settings2_screen.h"
#include "communication/espnow_handler.h"
#include "communication/mailbox.h"
#include "hardware/display.h"
#include "storage/preferences_manager.h"
#include "config.h"
#include <string.h>

// Guard time before committing a cycle-button selection to NVM/ESP-NOW.
// Long enough that the user can cycle past intermediate options without
// triggering intermediate writes.
#define CYCLE_WRITE_DEBOUNCE_MS 3000

static const char * const FUEL_NAMES[] = {
    "NO FUEL SENSOR", "ADC GAS", "GPIO EXPANDER", "ADC ELECTRIC"
};

// millis() timestamp of last fuel-type tap; 0 = no write pending
static uint32_t s_fuelWritePendingMs = 0;

// Same debounce for the mailbox pairing write.
static uint32_t s_mbxWritePendingMs = 0;

// lbl_pair_mbx is a fixed "PAIR MBX" literal owned by EEZ; C never writes it.
// lbl_mailbox_id_str carries the status text, also EEZ-owned via the
// mailbox_id_str variable — C drives only its colour, mirroring how
// updateEspnowGciMacColor() handles lbl_gci_mac_addr_value one row above.
static uint32_t s_mbxLabelMs   = 0;
static uint8_t  s_lastHealth   = 0xFF;   // force a write on first evaluation

// Mirrors LV_STATE_CHECKED on btn_pair_mailbox: green while an unclaimed offer
// is pending, i.e. exactly while tapping the button does something. Tracked so
// the state is only touched on a change — add/clear_state invalidates the object.
static bool     s_mbxOfferPending = false;

// True once the GPS row + home-loc section has been revealed this visit.
// Reset by settings2LoadStartCb so next entry always starts with the reduced widget set.
static bool s_gpsRowShown = false;

// Hides the home-loc section. Called at boot and via SCREEN_LOAD_START so
// widgets are hidden before the first render of every Settings2 visit, keeping peak
// draw-task heap low and preventing the OOM that plagued the original render pass.
//
// The sats/HDOP/lat/long labels used to be hidden here too, but they now live on
// the Settings screen and are owned by settings_screen.cpp. Touching them from
// here would toggle HIDDEN on objects belonging to an inactive screen — the
// invalidate trap described in settings2ScreenOnExit() — and they could never be
// revealed, because the pump below returns early unless Settings2 is active.
static void doHideGpsWidgets() {
    lv_obj_add_flag(objects.container_gps_hide, LV_OBJ_FLAG_HIDDEN);
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

// Claims the pending mailbox offer. SHORT_CLICKED, not CLICKED: LVGL 9 sends
// CLICKED on release regardless of press duration, so pairing it with the
// long-press handler below would re-accept an offer immediately after forgetting
// it. SHORT_CLICKED covers everything shorter than LONG_PRESS_TIME_MS, so there
// is no gap between the two gestures.
static void mbxAcceptCb(lv_event_t *) {
    if (mailboxAcceptOffer()) {
        tone_message();
        s_mbxWritePendingMs = millis();
    }
}

// Long press (LONG_PRESS_TIME_MS, set globally in initTouchscreen) forgets the
// paired mailbox. Confirms audibly, since the hold is long enough that you would
// otherwise not know when to let go.
static void mbxForgetCb(lv_event_t *) {
    mailboxForget();
    tone_confirm();
    s_mbxWritePendingMs = millis();
}

void settings2ScreenInit() {
    if (!objects.btn_fuel_sensor_type) return;
    lv_label_set_text(objects.lbl_fuel_sensor_type, FUEL_NAMES[fuelSensorType]);
    lv_obj_add_event_cb(objects.btn_fuel_sensor_type, fuelCycleCb, LV_EVENT_CLICKED, nullptr);

    if (objects.btn_pair_mailbox) {
        lv_obj_add_event_cb(objects.btn_pair_mailbox, mbxAcceptCb, LV_EVENT_SHORT_CLICKED, nullptr);
        lv_obj_add_event_cb(objects.btn_pair_mailbox, mbxForgetCb, LV_EVENT_LONG_PRESSED,  nullptr);
    }

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

    // Reveal the home-loc section once GPS has a position fix.
    // Un-hiding triggers incremental redraw of only the newly visible area — no heap spike.
    if (!s_gpsRowShown && fix.valid.location) {
        lv_obj_clear_flag(objects.container_gps_hide, LV_OBJ_FLAG_HIDDEN);
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

    if (s_mbxWritePendingMs != 0 &&
        (millis() - s_mbxWritePendingMs) >= CYCLE_WRITE_DEBOUNCE_MS) {
        queuePreferenceWrite("mbx_id", (int)mailboxGetPairedId());
        s_mbxWritePendingMs = 0;
    }

    // Refresh the pair button at most once a second, and only while this screen
    // is active — writing a label invalidates it, and invalidating an object on
    // an inactive screen queues dirty regions that show up as sluggish transitions.
    uint32_t now = millis();
    if ((now - s_mbxLabelMs) >= 1000) {
        s_mbxLabelMs = now;

        // Colour of the id string: white awaiting / unpaired, green healthy,
        // yellow with a miss count, red after 24 h of silence. Only written on a
        // change — setting a style invalidates the object.
        uint8_t h = mailboxHealth();
        if (h != s_lastHealth && objects.lbl_mailbox_id_str) {
            s_lastHealth = h;
            lv_color_t c = (h == MBX_HEALTH_OK)     ? lv_color_hex(0xff00ff2d)
                         : (h == MBX_HEALTH_MISSED) ? lv_color_hex(0xffffff00)
                         : (h == MBX_HEALTH_SILENT) ? lv_color_hex(0xffff0000)
                                                    : lv_color_white();
            lv_obj_set_style_text_color(objects.lbl_mailbox_id_str, c,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        // Button turns green while there is something to claim, so it is
        // highlighted exactly when pressing it does something.
        uint32_t offer = mailboxFreshOfferId();
        bool pending = (offer != 0 && offer != mailboxGetPairedId());
        if (pending != s_mbxOfferPending) {
            s_mbxOfferPending = pending;
            if (objects.btn_pair_mailbox) {
                if (pending) lv_obj_add_state(objects.btn_pair_mailbox, LV_STATE_CHECKED);
                else         lv_obj_clear_state(objects.btn_pair_mailbox, LV_STATE_CHECKED);
            }
        }
    }
}

void settings2ScreenOnExit() {
    if (s_fuelWritePendingMs != 0) {
        queuePreferenceWrite("fuel_sense_type", (int)fuelSensorType);
        espNow.sendFuelConfig();
        s_fuelWritePendingMs = 0;
    }
    if (s_mbxWritePendingMs != 0) {
        queuePreferenceWrite("mbx_id", (int)mailboxGetPairedId());
        s_mbxWritePendingMs = 0;
    }
    // No LVGL calls here — settings2LoadStartCb re-hides GPS widgets before
    // the next Settings2 render. See memory note "lvgl-offscreen-invalidate":
    // lv_obj_add_flag on inactive-screen objects queues dirty regions and
    // corrupts the Home screen refresh that follows this exit.
}
