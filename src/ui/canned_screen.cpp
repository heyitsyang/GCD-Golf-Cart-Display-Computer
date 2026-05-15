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

#define TIMESTAMP_VALID_MIN 1500000000U
#define CTX_PREFIX_WIDTH    52
#define CTX_ROW_HEIGHT      32

typedef enum { HUB_MODE_REPLY, HUB_MODE_NEW } hub_mode_t;

static hub_mode_t  s_mode      = HUB_MODE_NEW;
static uint8_t     s_channel   = 0;
static uint32_t    s_dest      = BROADCAST_ADDR;
static chatMessage_t s_srcMsg  = {};
static bool        s_hasSrcMsg = false;

// Hand-coded LVGL children of canned_msgs_container_body. Built
// lazily on first navigation to the canned hub.
static lv_obj_t *s_contextRow    = nullptr;
static lv_obj_t *s_contextPrefix = nullptr;
static lv_obj_t *s_contextMsg   = nullptr;
static lv_obj_t *s_channelBtn   = nullptr;
static lv_obj_t *s_channelLabel = nullptr;
static lv_obj_t *s_slotBtns[CANNED_REPLY_COUNT]   = {nullptr};
static lv_obj_t *s_slotLabels[CANNED_REPLY_COUNT] = {nullptr};

static void buildContextPrefix(const chatMessage_t *m, char *out, size_t outSize) {
    char hhmm[10];
    if (m->timestamp < TIMESTAMP_VALID_MIN) {
        snprintf(hhmm, sizeof(hhmm), "--:--");
    } else {
        time_t t = (time_t)m->timestamp;
        struct tm tmv;
        if (localtime_r(&t, &tmv)) {
            int h = tmv.tm_hour;
            char ap = (h < 12) ? 'a' : 'p';
            int h12 = h % 12;
            if (h12 == 0) h12 = 12;
            snprintf(hhmm, sizeof(hhmm), "%d:%02d%c", h12, tmv.tm_min, ap);
        } else {
            snprintf(hhmm, sizeof(hhmm), "--:--");
        }
    }
    uint32_t addr = m->outgoing ? m->to : m->from;
    snprintf(out, outSize, "%u %s\n!%04x",
             (unsigned)m->channel, hhmm,
             (unsigned)(addr & 0xFFFF));
}

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

    // Context row — flex-row with prefix (montserrat_12) + scrolling message.
    // Shown in REPLY mode; hidden in NEW mode.
    s_contextRow = lv_obj_create(body);
    lv_obj_set_pos(s_contextRow, 0, 0);
    lv_obj_set_size(s_contextRow, 316, CTX_ROW_HEIGHT);
    lv_obj_set_style_bg_color(s_contextRow, lv_color_hex(0x101010), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_contextRow, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_contextRow, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_contextRow, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_contextRow, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_column(s_contextRow, 2, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_contextRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_contextRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    s_contextPrefix = lv_label_create(s_contextRow);
    lv_obj_set_width(s_contextPrefix, CTX_PREFIX_WIDTH);
    lv_obj_set_height(s_contextPrefix, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(s_contextPrefix, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_contextPrefix, lv_color_white(), LV_PART_MAIN);
    lv_label_set_long_mode(s_contextPrefix, LV_LABEL_LONG_CLIP);
    lv_label_set_text(s_contextPrefix, "");

    s_contextMsg = lv_label_create(s_contextRow);
    lv_obj_set_flex_grow(s_contextMsg, 1);
    lv_obj_set_height(s_contextMsg, LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(s_contextMsg, lv_color_white(), LV_PART_MAIN);
    lv_label_set_long_mode(s_contextMsg, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(s_contextMsg, "");

    // Channel cycler (NEW mode only).
    s_channelBtn = lv_btn_create(body);
    lv_obj_set_pos(s_channelBtn, 0, 34);
    lv_obj_set_size(s_channelBtn, 70, 22);
    lv_obj_set_style_bg_color(s_channelBtn, lv_color_hex(0x003c6b), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(s_channelBtn, lv_color_hex(0x003c6b), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_channelBtn, lv_color_hex(0x002d50), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_color(s_channelBtn, lv_color_hex(0x002d50), LV_PART_MAIN | LV_STATE_PRESSED);
    s_channelLabel = lv_label_create(s_channelBtn);
    lv_label_set_text(s_channelLabel, "Ch 0");
    lv_obj_center(s_channelLabel);
    lv_obj_add_event_cb(s_channelBtn, channelCycleCb, LV_EVENT_CLICKED, nullptr);

    // 8 slot buttons in 2 cols × 4 rows.
    const int gridX0 = 0;
    const int gridY0 = 58;
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
    if (s_contextRow) {
        if (s_mode == HUB_MODE_REPLY && s_hasSrcMsg) {
            char prefix[24];
            buildContextPrefix(&s_srcMsg, prefix, sizeof(prefix));
            lv_label_set_text(s_contextPrefix, prefix);
            lv_label_set_text(s_contextMsg, s_srcMsg.text);
            lv_obj_clear_flag(s_contextRow, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_contextRow, LV_OBJ_FLAG_HIDDEN);
        }
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

void cannedScreenSetReplyMode(uint8_t channel, uint32_t dest, const chatMessage_t *srcMsg) {
    s_mode      = HUB_MODE_REPLY;
    s_channel   = channel;
    s_dest      = dest;
    s_hasSrcMsg = (srcMsg != nullptr);
    if (srcMsg) s_srcMsg = *srcMsg;
    s_uiDirty   = true;
}

void cannedScreenSetNewMode() {
    s_mode      = HUB_MODE_NEW;
    s_dest      = BROADCAST_ADDR;
    s_hasSrcMsg = false;
    s_uiDirty   = true;
}

void cannedScreenPump() {
    if (lv_scr_act() != objects.meshtastic_canned_messages) return;
    if (!s_uiDirty) return;
    s_uiDirty = false;
    applyModeUI();
}
