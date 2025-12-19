#ifndef TONE_ACTIONS_H
#define TONE_ACTIONS_H

#include <lvgl.h>

// External C functions called from EEZ Studio actions
extern "C" void action_tone_message(lv_event_t *e);
extern "C" void action_tone_alert(lv_event_t *e);
extern "C" void action_tone_urgent(lv_event_t *e);
extern "C" void action_tone_confirm(lv_event_t *e);
extern "C" void action_tone_click(lv_event_t *e);
extern "C" void action_tone_error(lv_event_t *e);

#endif // TONE_ACTIONS_H
