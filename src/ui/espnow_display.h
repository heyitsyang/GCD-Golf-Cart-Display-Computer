#ifndef ESPNOW_DISPLAY_H
#define ESPNOW_DISPLAY_H

#include <lvgl.h>

void displayESPNowStatus(lv_obj_t* parent);
void updateESPNowDisplay();
void showESPNowPeerList();
void showESPNowAddPeerDialog();
void showESPNowSendDialog();

// EEZ Studio action handlers
extern "C" void action_espnow_toggle(lv_event_t *e);
extern "C" void action_espnow_add_peer(lv_event_t *e);
extern "C" void action_espnow_remove_peer(lv_event_t *e);
extern "C" void action_espnow_send_message(lv_event_t *e);
extern "C" void action_espnow_show_peers(lv_event_t *e);

#endif // ESPNOW_DISPLAY_H