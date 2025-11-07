#include "espnow_display.h"
#include "config.h"
#include "globals.h"
#include "communication/espnow_handler.h"
#include "types.h"

static lv_obj_t* status_label = nullptr;
static lv_obj_t* peer_count_label = nullptr;
static lv_obj_t* last_msg_label = nullptr;

// Static callback functions to avoid lambda issues
static void close_btn_event_cb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t* modal = lv_obj_get_parent(btn);
    lv_obj_del(modal);
}

static void add_peer_btn_cb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t* btn_cont = lv_obj_get_parent(btn);
    lv_obj_t* modal = lv_obj_get_parent(btn_cont);
    
    // Find textarea (it should be child 1 of modal)
    lv_obj_t* textarea = lv_obj_get_child(modal, 1);
    
    if (textarea) {
        const char* text = lv_textarea_get_text(textarea);
        String mac_str = String(text);
        if (espNow.addPeerFromString(mac_str, "New Peer")) {
            // Save to EEPROM
            espnow_gci_mac_addr = mac_str;
            lv_obj_del(modal);
        }
    }
}

static void cancel_btn_cb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t* btn_cont = lv_obj_get_parent(btn);
    lv_obj_t* modal = lv_obj_get_parent(btn_cont);
    lv_obj_del(modal);
}

static void send_msg_btn_cb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t* btn_cont = lv_obj_get_parent(btn);
    lv_obj_t* modal = lv_obj_get_parent(btn_cont);
    
    // Find textarea (it should be child 1 of modal)
    lv_obj_t* textarea = lv_obj_get_child(modal, 1);
    
    if (textarea) {
        const char* text = lv_textarea_get_text(textarea);
        String message = String(text);
        if (message.length() > 0) {
            espNow.broadcastText(message);
            lv_obj_del(modal);
        }
    }
}

void displayESPNowStatus(lv_obj_t* parent) {
    // Create status container
    lv_obj_t* container = lv_obj_create(parent);
    lv_obj_set_size(container, 280, 80);
    lv_obj_align(container, LV_ALIGN_TOP_MID, 0, 10);
    
    // Status label
    status_label = lv_label_create(container);
    lv_label_set_text(status_label, "ESP-NOW: Not initialized");
    lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 10, 10);
    
    // Peer count label
    peer_count_label = lv_label_create(container);
    lv_label_set_text(peer_count_label, "Peers: 0");
    lv_obj_align(peer_count_label, LV_ALIGN_TOP_LEFT, 10, 30);
    
    // Last message label
    last_msg_label = lv_label_create(container);
    lv_label_set_text(last_msg_label, "No messages");
    lv_obj_align(last_msg_label, LV_ALIGN_TOP_LEFT, 10, 50);
}

void updateESPNowDisplay() {
    if (status_label) {
        String status = "ESP-NOW: " + espnow_status;
        lv_label_set_text(status_label, status.c_str());
    }
    
    if (peer_count_label) {
        String peers = "Peers: " + String(espnow_peer_count);
        lv_label_set_text(peer_count_label, peers.c_str());
    }
    
    if (last_msg_label && espnow_last_received.length() > 0) {
        lv_label_set_text(last_msg_label, espnow_last_received.c_str());
    }
}

void showESPNowPeerList() {
    lv_obj_t* current_screen = lv_scr_act();
    
    // Create modal window
    lv_obj_t* modal = lv_obj_create(current_screen);
    lv_obj_set_size(modal, 300, 200);
    lv_obj_align(modal, LV_ALIGN_CENTER, 0, 0);
    
    // Title
    lv_obj_t* title = lv_label_create(modal);
    lv_label_set_text(title, "ESP-NOW Peers");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Create list
    lv_obj_t* list = lv_list_create(modal);
    lv_obj_set_size(list, 280, 120);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 10);
    
    // Add peers to list
    for (int i = 0; i < espNow.getPeerCount(); i++) {
        espnow_peer_info_t* peer = espNow.getPeerInfo(i);
        if (peer) {
            char text[64];
            snprintf(text, sizeof(text), "%s (%s) RSSI:%d", 
                    peer->name,
                    peer->is_online ? "Online" : "Offline",
                    peer->last_rssi);
            lv_list_add_text(list, text);
        }
    }
    
    // Close button
    lv_obj_t* close_btn = lv_btn_create(modal);
    lv_obj_set_size(close_btn, 80, 30);
    lv_obj_align(close_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_t* close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "Close");
    lv_obj_center(close_label);
    
    lv_obj_add_event_cb(close_btn, close_btn_event_cb, LV_EVENT_CLICKED, NULL);
}

void showESPNowAddPeerDialog() {
    lv_obj_t* current_screen = lv_scr_act();
    
    // Create modal window
    lv_obj_t* modal = lv_obj_create(current_screen);
    lv_obj_set_size(modal, 280, 150);
    lv_obj_align(modal, LV_ALIGN_CENTER, 0, 0);
    
    // Title
    lv_obj_t* title = lv_label_create(modal);
    lv_label_set_text(title, "Add ESP-NOW Peer");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // MAC address input
    lv_obj_t* textarea = lv_textarea_create(modal);
    lv_obj_set_size(textarea, 240, 40);
    lv_obj_align(textarea, LV_ALIGN_CENTER, 0, -10);
    lv_textarea_set_placeholder_text(textarea, "XX:XX:XX:XX:XX:XX");
    lv_textarea_set_max_length(textarea, 17);
    
    // Button container
    lv_obj_t* btn_cont = lv_obj_create(modal);
    lv_obj_set_size(btn_cont, 240, 40);
    lv_obj_align(btn_cont, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    
    // Add button
    lv_obj_t* add_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(add_btn, 100, 30);
    lv_obj_align(add_btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t* add_label = lv_label_create(add_btn);
    lv_label_set_text(add_label, "Add");
    lv_obj_center(add_label);
    
    // Cancel button
    lv_obj_t* cancel_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(cancel_btn, 100, 30);
    lv_obj_align(cancel_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_t* cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);
    
    // Add event callbacks
    lv_obj_add_event_cb(add_btn, add_peer_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(cancel_btn, cancel_btn_cb, LV_EVENT_CLICKED, NULL);
}

void showESPNowSendDialog() {
    lv_obj_t* current_screen = lv_scr_act();
    
    // Create modal window
    lv_obj_t* modal = lv_obj_create(current_screen);
    lv_obj_set_size(modal, 300, 180);
    lv_obj_align(modal, LV_ALIGN_CENTER, 0, 0);
    
    // Title
    lv_obj_t* title = lv_label_create(modal);
    lv_label_set_text(title, "Send ESP-NOW Message");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Message input
    lv_obj_t* textarea = lv_textarea_create(modal);
    lv_obj_set_size(textarea, 280, 60);
    lv_obj_align(textarea, LV_ALIGN_CENTER, 0, -10);
    lv_textarea_set_placeholder_text(textarea, "Enter message...");
    lv_textarea_set_max_length(textarea, 200);
    
    // Button container
    lv_obj_t* btn_cont = lv_obj_create(modal);
    lv_obj_set_size(btn_cont, 280, 40);
    lv_obj_align(btn_cont, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    
    // Send button
    lv_obj_t* send_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(send_btn, 100, 30);
    lv_obj_align(send_btn, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_t* send_label = lv_label_create(send_btn);
    lv_label_set_text(send_label, "Broadcast");
    lv_obj_center(send_label);
    
    // Cancel button
    lv_obj_t* cancel_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(cancel_btn, 100, 30);
    lv_obj_align(cancel_btn, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_t* cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);
    
    // Add event callbacks
    lv_obj_add_event_cb(send_btn, send_msg_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(cancel_btn, cancel_btn_cb, LV_EVENT_CLICKED, NULL);
}

// EEZ Studio action handlers
extern "C" void action_espnow_toggle(lv_event_t *e) {
    espnow_enabled = !espnow_enabled;
    updateESPNowDisplay();
}

extern "C" void action_espnow_add_peer(lv_event_t *e) {
    showESPNowAddPeerDialog();
}

extern "C" void action_espnow_remove_peer(lv_event_t *e) {
    // Implement peer removal dialog if needed
}

extern "C" void action_espnow_send_message(lv_event_t *e) {
    showESPNowSendDialog();
}

extern "C" void action_espnow_show_peers(lv_event_t *e) {
    showESPNowPeerList();
}