#include <lvgl.h>
#include "ui_eez/screens.h"
#include "menu_screen.h"

// True once btn_mtrx_menu has been revealed after the first render of each Menu visit.
// Reset on LV_EVENT_SCREEN_LOAD_START so the first frame always renders without the matrix.
static bool s_matrixShown = false;

// Fires on LV_EVENT_SCREEN_LOAD_START for every Menu visit.
// Hiding here (on the loading — soon-to-be-active — screen) keeps the call legal
// and ensures btn_mtrx_menu is absent from the first full-screen draw pass.
// This reduces draw task count from ~150/tile to ~20/tile, keeping the heap's
// largest free block above 640 bytes so the WiFi pvPortMalloc race cannot crash us.
static void menuLoadStartCb(lv_event_t *) {
    lv_obj_add_flag(objects.btn_mtrx_menu, LV_OBJ_FLAG_HIDDEN);
    s_matrixShown = false;
}

void menuScreenInit() {
    if (!objects.btn_mtrx_menu) return;
    lv_obj_add_event_cb(objects.menu, menuLoadStartCb, LV_EVENT_SCREEN_LOAD_START, nullptr);
    // Initial hide: menu is pre-created but not yet loaded.
    lv_obj_add_flag(objects.btn_mtrx_menu, LV_OBJ_FLAG_HIDDEN);
}

void menuScreenPump() {
    if (lv_scr_act() != objects.menu) return;
    if (s_matrixShown) return;
    lv_obj_clear_flag(objects.btn_mtrx_menu, LV_OBJ_FLAG_HIDDEN);
    s_matrixShown = true;
}
