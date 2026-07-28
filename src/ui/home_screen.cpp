#include <lvgl.h>
#include "ui_eez/screens.h"
#include "home_screen.h"
#include "communication/mailbox.h"

// Long-pressing the mail glyph extinguishes it, so a misbehaving sensor can be
// silenced without a reboot. Long press rather than tap because the glyph sits
// in the middle of the Home screen where stray taps are easy, and dismissing is
// the one action here that discards information. Uses the global
// LONG_PRESS_TIME_MS threshold set in initTouchscreen().
static void mailGlyphLongPressedCb(lv_event_t *) {
    if (!mailboxGlyphOn()) return;   // nothing lit; ignore stray presses
    mailboxDismiss();
}

void homeScreenInit() {
    if (!objects.lbl_mail_available) return;

    // The label is LV_SIZE_CONTENT, so while the glyph is dark its hit area is
    // zero — you cannot dismiss what is already off. The ext click area only
    // enlarges the target once there is a glyph to hit.
    lv_obj_add_flag(objects.lbl_mail_available, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(objects.lbl_mail_available, 8);
    lv_obj_add_event_cb(objects.lbl_mail_available, mailGlyphLongPressedCb,
                        LV_EVENT_LONG_PRESSED, nullptr);

    // LV_OBJ_FLAG_GESTURE_BUBBLE is left alone on purpose: a swipe that starts
    // on the label must still resolve up to objects.home so screen gestures work.
}
