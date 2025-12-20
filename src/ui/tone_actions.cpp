#include "tone_actions.h"
#include "hardware/display.h"

// EEZ Studio action wrappers for tone functions
// These are called from the EEZ Studio UI when buttons/actions are triggered

extern "C" void action_tone_message(lv_event_t *e) {
    tone_message();
}

extern "C" void action_tone_alert(lv_event_t *e) {
    tone_alert();
}

extern "C" void action_tone_urgent(lv_event_t *e) {
    tone_urgent();
}

extern "C" void action_tone_confirm(lv_event_t *e) {
    tone_confirm();
}

extern "C" void action_tone_click(lv_event_t *e) {
    tone_click();
}

extern "C" void action_tone_error(lv_event_t *e) {
    tone_error();
}
