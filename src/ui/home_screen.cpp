#include <lvgl.h>
#include "ui_eez/screens.h"
#include "home_screen.h"
#include "communication/mailbox.h"

// Tapping the mail glyph extinguishes it until the mailbox next changes state,
// so a misbehaving sensor can be silenced without a reboot.
static void mailGlyphClickedCb(lv_event_t *) {
    if (!mailboxGlyphOn()) return;   // nothing lit; ignore stray taps
    mailboxDismiss();
}

void homeScreenInit() {
    if (!objects.lbl_mail_available) return;

    // The label is LV_SIZE_CONTENT, so while the glyph is dark its hit area is
    // zero — you cannot dismiss what is already off. The ext click area only
    // enlarges the target once there is a glyph to hit.
    lv_obj_add_flag(objects.lbl_mail_available, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(objects.lbl_mail_available, 8);
    lv_obj_add_event_cb(objects.lbl_mail_available, mailGlyphClickedCb,
                        LV_EVENT_CLICKED, nullptr);

    // LV_OBJ_FLAG_GESTURE_BUBBLE is left alone on purpose: a swipe that starts
    // on the label must still resolve up to objects.home so screen gestures work.
}
