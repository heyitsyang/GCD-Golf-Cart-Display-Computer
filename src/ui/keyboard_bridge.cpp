#include "keyboard_bridge.h"
#include "config.h"
#include "globals.h"
#include "ui_eez/screens.h"
#include "ui_eez/eez-flow.h"
#include <lvgl.h>

static kb_mode_t       s_mode      = KB_MODE_CHAT;
static volatile bool   s_active    = false;
static kb_done_cb_t    s_on_done   = nullptr;
static kb_cancel_cb_t  s_on_cancel = nullptr;
static void           *s_userdata  = nullptr;

static void clearState() {
    s_active    = false;
    s_on_done   = nullptr;
    s_on_cancel = nullptr;
    s_userdata  = nullptr;
}

// Fires after the EEZ flow's Full_Keyboard.READY chain (tone_click →
// IsTrue → SetVariable text_message). The chain no longer pops the
// screen in v1.6, so the bridge owns post-Enter behavior.
static void kb_ready_cb(lv_event_t *e) {
    if (!s_active || !s_on_done) return;
    if (!objects.output_keyboard) return;

    const char *typed = lv_textarea_get_text(objects.output_keyboard);
    if (!typed) typed = "";

    // Snapshot mode/cb in case on_done re-arms via kb_request().
    kb_mode_t   mode    = s_mode;
    kb_done_cb_t done   = s_on_done;
    void       *userdata = s_userdata;

    if (mode == KB_MODE_CANNED_EDIT) {
        // One-shot: clear before invoking so a re-entrant kb_request
        // from inside on_done isn't clobbered.
        clearState();
        done(typed, userdata);
        eez_flow_pop_screen(LV_SCR_LOAD_ANIM_NONE, 200, 0);
    } else {
        // Sticky chat: keep state armed; on_done is responsible for TX,
        // chat buffer append, and updating the transcript via
        // kb_set_context_append().
        done(typed, userdata);
    }
}

// Fires after the EEZ flow's go_to_menu_11.CLICKED chain
// (tone_click → changeScreen "Meshtastic Messages").
static void kb_cancel_cb_handler(lv_event_t *e) {
    if (!s_active) return;

    kb_cancel_cb_t cancel = s_on_cancel;
    void *userdata = s_userdata;
    clearState();
    if (cancel) cancel(userdata);
}

void kb_init() {
    if (objects.full_keyboard) {
        lv_obj_add_event_cb(objects.full_keyboard, kb_ready_cb,
                            LV_EVENT_READY, nullptr);
    }
    if (objects.go_to_menu_11) {
        lv_obj_add_event_cb(objects.go_to_menu_11, kb_cancel_cb_handler,
                            LV_EVENT_CLICKED, nullptr);
    }
}

static void armState(kb_mode_t       mode,
                     const char     *initial,
                     const char     *context,
                     kb_done_cb_t    on_done,
                     kb_cancel_cb_t  on_cancel,
                     void           *userdata) {
    s_mode      = mode;
    s_active    = true;
    s_on_done   = on_done;
    s_on_cancel = on_cancel;
    s_userdata  = userdata;

    if (objects.output_keyboard) {
        lv_textarea_set_text(objects.output_keyboard, initial ? initial : "");
    }
    if (objects.received_msg) {
        lv_textarea_set_text(objects.received_msg, context ? context : "");
        lv_obj_add_flag(objects.received_msg, LV_OBJ_FLAG_SCROLLABLE);
    }
}

void kb_arm_chat(const char     *initial,
                 const char     *context,
                 kb_done_cb_t    on_done,
                 kb_cancel_cb_t  on_cancel,
                 void           *userdata) {
    armState(KB_MODE_CHAT, initial, context, on_done, on_cancel, userdata);
    // Caller's EEZ flow handles the screen change.
}

void kb_request_canned_edit(const char  *initial,
                            const char  *context,
                            kb_done_cb_t on_done,
                            void        *userdata) {
    armState(KB_MODE_CANNED_EDIT, initial, context, on_done, nullptr, userdata);
    eez_flow_push_screen(SCREEN_ID_KEYBOARD_ENTRY,
                         LV_SCR_LOAD_ANIM_FADE_IN, 200, 0);
}

static void enqueueContext(bool append, const char *text) {
    if (!kbContextQueue || !text) return;
    kb_ctx_item_t item;
    item.append = append;
    strncpy(item.text, text, sizeof(item.text) - 1);
    item.text[sizeof(item.text) - 1] = '\0';
    xQueueSend(kbContextQueue, &item, 0);
}

void kb_set_context(const char *text)        { enqueueContext(false, text); }
void kb_set_context_append(const char *text) { enqueueContext(true,  text); }

void kb_pump() {
    if (!kbContextQueue) return;
    kb_ctx_item_t item;
    while (xQueueReceive(kbContextQueue, &item, 0) == pdTRUE) {
        if (lv_scr_act() != objects.keyboard_entry) continue;
        if (!objects.received_msg) continue;
        if (item.append) {
            const char *existing = lv_textarea_get_text(objects.received_msg);
            String combined = String(existing ? existing : "") + item.text;
            lv_textarea_set_text(objects.received_msg, combined.c_str());
        } else {
            lv_textarea_set_text(objects.received_msg, item.text);
        }
    }
}

bool kb_is_active()           { return s_active; }
kb_mode_t kb_current_mode()   { return s_mode; }
