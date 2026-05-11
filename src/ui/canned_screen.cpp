#include "canned_screen.h"
#include "canned_replies.h"
#include "communication/chat_buffer.h"
#include "config.h"
#include "globals.h"
#include "ui/chat_screen.h"
#include "ui_eez/screens.h"
#include "ui_eez/eez-flow.h"
#include <Meshtastic.h>
#include <lvgl.h>
#include <time.h>

typedef enum { HUB_MODE_REPLY, HUB_MODE_NEW } hub_mode_t;

static hub_mode_t s_mode      = HUB_MODE_NEW;
static uint8_t    s_channel   = 0;
static uint32_t   s_dest      = BROADCAST_ADDR;
static String     s_context;

// Hand-coded LVGL children of canned_msgs_container_body. Built
// lazily on first navigation to the canned hub.
static lv_obj_t *s_contextStrip = nullptr;
static lv_obj_t *s_channelBtn   = nullptr;
static lv_obj_t *s_channelLabel = nullptr;
static lv_obj_t *s_slotBtns[CANNED_REPLY_COUNT]   = {nullptr};
static lv_obj_t *s_slotLabels[CANNED_REPLY_COUNT] = {nullptr};

static volatile bool s_uiDirty   = true;
static bool          s_bodyBuilt = false;

static void enqueueTx(uint8_t channel, uint32_t dest, const char *text) {
    if (!chatTxQueue || !text || !*text) return;

    chatTxItem_t tx;
    tx.channel = channel;
    tx.dest    = dest;
    strncpy(tx.text, text, sizeof(tx.text) - 1);
    tx.text[sizeof(tx.text) - 1] = '\0';
    if (xQueueSend(chatTxQueue, &tx, 0) != pdTRUE) {
        Serial.println("chatTxQueue full, drop");
        return;
    }

    chatMessage_t cm = {};
    cm.from      = 0;
    cm.to        = dest;
    cm.channel   = channel;
    cm.timestamp = (uint32_t)time(NULL);
    cm.outgoing  = true;
    strncpy(cm.text, text, sizeof(cm.text) - 1);
    cm.text[sizeof(cm.text) - 1] = '\0';
    chatBufferAppend(&cm);
    chatScreenRequestRefresh();
}

static void slotClickedCb(lv_event_t *e) {
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
    uint8_t idx = (uint8_t)(uintptr_t)lv_obj_get_user_data(btn);
    if (idx >= CANNED_REPLY_COUNT) return;

    enqueueTx(s_channel, s_dest, cannedReplyGet(idx));
    eez_flow_pop_screen(LV_SCR_LOAD_ANIM_NONE, 200, 0);
}

static void channelCycleCb(lv_event_t *e) {
    s_channel = (s_channel + 1) % 3;
    if (s_channelLabel) {
        char buf[8];
        snprintf(buf, sizeof(buf), "Ch %u", (unsigned)s_channel);
        lv_label_set_text(s_channelLabel, buf);
    }
}

static void buildBody() {
    if (s_bodyBuilt) return;
    if (!objects.canned_msgs_container_body) return;
    lv_obj_t *body = objects.canned_msgs_container_body;

    lv_obj_set_style_bg_color(body, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(body, 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(body, 0, LV_PART_MAIN);

    // Context strip — read-only textarea at top of body.
    s_contextStrip = lv_textarea_create(body);
    lv_obj_set_pos(s_contextStrip, 0, 0);
    lv_obj_set_size(s_contextStrip, 316, 38);
    lv_textarea_set_one_line(s_contextStrip, false);
    lv_textarea_set_text(s_contextStrip, "");
    lv_obj_add_state(s_contextStrip, LV_STATE_DISABLED);
    lv_obj_set_style_text_color(s_contextStrip, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_contextStrip, lv_color_hex(0x101010), LV_PART_MAIN);

    // Channel cycler (NEW mode only).
    s_channelBtn = lv_btn_create(body);
    lv_obj_set_pos(s_channelBtn, 0, 42);
    lv_obj_set_size(s_channelBtn, 70, 22);
    s_channelLabel = lv_label_create(s_channelBtn);
    lv_label_set_text(s_channelLabel, "Ch 0");
    lv_obj_center(s_channelLabel);
    lv_obj_add_event_cb(s_channelBtn, channelCycleCb, LV_EVENT_CLICKED, nullptr);

    // 8 slot buttons in 2 cols × 4 rows.
    const int gridX0 = 0;
    const int gridY0 = 68;
    const int colW   = 156;
    const int rowH   = 24;
    const int hgap   = 4;
    const int vgap   = 4;
    for (uint8_t i = 0; i < CANNED_REPLY_COUNT; i++) {
        int col = i % 2;
        int row = i / 2;
        int x = gridX0 + col * (colW + hgap);
        int y = gridY0 + row * (rowH + vgap);

        lv_obj_t *btn = lv_btn_create(body);
        lv_obj_set_pos(btn, x, y);
        lv_obj_set_size(btn, colW, rowH);
        lv_obj_set_user_data(btn, (void *)(uintptr_t)i);
        lv_obj_add_event_cb(btn, slotClickedCb, LV_EVENT_CLICKED, nullptr);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl, colW - 8);
        lv_obj_center(lbl);
        lv_label_set_text(lbl, cannedReplyGet(i));

        s_slotBtns[i]   = btn;
        s_slotLabels[i] = lbl;
    }

    s_bodyBuilt = true;
}

void cannedScreenInit() {
    buildBody();
    cannedScreenSetNewMode();
}

static void applyModeUI() {
    if (s_contextStrip) {
        lv_textarea_set_text(s_contextStrip, s_context.c_str());
    }
    if (s_channelBtn) {
        if (s_mode == HUB_MODE_NEW) {
            lv_obj_clear_flag(s_channelBtn, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_channelBtn, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (s_channelLabel) {
        char buf[8];
        snprintf(buf, sizeof(buf), "Ch %u", (unsigned)s_channel);
        lv_label_set_text(s_channelLabel, buf);
    }
}

void cannedScreenSetReplyMode(uint8_t channel, uint32_t dest, const char *contextText) {
    s_mode    = HUB_MODE_REPLY;
    s_channel = channel;
    s_dest    = dest;
    s_context = contextText ? contextText : "";
    s_uiDirty = true;
}

void cannedScreenSetNewMode() {
    s_mode    = HUB_MODE_NEW;
    s_dest    = BROADCAST_ADDR;
    s_context = "";
    s_uiDirty = true;
}

void cannedScreenPump() {
    if (lv_scr_act() != objects.meshtastic_canned_messages) return;
    if (!s_uiDirty) return;
    s_uiDirty = false;
    applyModeUI();
}
