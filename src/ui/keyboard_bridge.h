#ifndef KEYBOARD_BRIDGE_H
#define KEYBOARD_BRIDGE_H

#include <Arduino.h>
#include "config.h"

// kbContextQueue payload (cross-task RX -> GUI transcript marshaling).
typedef struct {
    bool append;  // true = append, false = replace
    char text[MAX_MESHTASTIC_PAYLOAD];
} kb_ctx_item_t;

typedef void (*kb_done_cb_t)(const char *text, void *userdata);
typedef void (*kb_cancel_cb_t)(void *userdata);

typedef enum {
    KB_MODE_CHAT,         // sticky: Enter sends, screen stays;
                          //   back arrow → Meshtastic Messages (EEZ flow handles nav)
    KB_MODE_CANNED_EDIT,  // one-shot: Enter saves, bridge pops to hub
} kb_mode_t;

// One-time wiring. Call from setup() AFTER ui_init(). Adds extra
// LV_EVENT callbacks to objects.full_keyboard (READY) and
// objects.go_to_menu_11 (CLICKED) on the Keyboard_Entry screen.
void kb_init();

// Arm bridge state for sticky chat mode. Pre-fills output_keyboard
// and received_msg, but does NOT push the screen — used when the
// caller is hooked onto a button whose EEZ flow already navigates to
// Keyboard_Entry (e.g. btn_kb_open on the canned hub). Must be
// called from the GUI task.
void kb_arm_chat(const char     *initial,
                 const char     *context,
                 kb_done_cb_t    on_done,
                 kb_cancel_cb_t  on_cancel,
                 void           *userdata);

// Arm bridge state for canned-edit mode AND push the Keyboard_Entry
// screen via the EEZ flow runtime. Used for the hand-coded canned
// slot buttons in EDIT mode (no EEZ flow on those buttons).
void kb_request_canned_edit(const char  *initial,
                            const char  *context,
                            kb_done_cb_t on_done,
                            void        *userdata);

// Replace or append text in received_msg (the transcript / context
// textarea on Keyboard_Entry). Safe to call from any task; updates
// are marshaled through kbContextQueue and applied by kb_pump() on
// the GUI task.
void kb_set_context(const char *text);
void kb_set_context_append(const char *text);

// Drain queued context updates and apply to received_msg.
// Call from the GUI task each tick.
void kb_pump();

bool kb_is_active();
kb_mode_t kb_current_mode();

#endif // KEYBOARD_BRIDGE_H
