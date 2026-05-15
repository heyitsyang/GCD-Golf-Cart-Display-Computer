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
static lv_obj_t *s_channelDd = nullptr;
// s_destDd goes here in the future destination-dropdown effort

#define GCM_MAX_CHANNELS 8
static char    s_chanNames[GCM_MAX_CHANNELS][12] = {};
static uint8_t s_chanCount = 3;
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

void cannedScreenOnChannelResponse(const meshtastic_Channel *ch) {
    if (!ch || ch->index < 0 || ch->index >= GCM_MAX_CHANNELS) return;
    if (ch->role == meshtastic_Channel_Role_DISABLED) return;
    const char *name = (ch->settings.name[0] != '\0') ? ch->settings.name : "LongFast";
    strncpy(s_chanNames[ch->index], name, 11);
    s_chanNames[ch->index][11] = '\0';
    if ((uint8_t)(ch->index + 1) > s_chanCount)
        s_chanCount = (uint8_t)(ch->index + 1);
    s_uiDirty = true;
}

void cannedScreenResetChannelNames() {
    memset(s_chanNames, 0, sizeof(s_chanNames));
    s_chanCount = 3;
}

// Creates and styles a selector dropdown (channel, and future destination).
// Caller sets options and wires LV_EVENT_VALUE_CHANGED.
static lv_obj_t *makeSelectorDropdown(lv_obj_t *parent, int x, int w) {
    lv_obj_t *dd = lv_dropdown_create(parent);
    lv_obj_set_pos(dd, x, 34);
    lv_obj_set_size(dd, w, 28);
    lv_obj_set_style_pad_all(dd, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(dd, lv_color_hex(0x003c6b), LV_PART_MAIN);
    lv_obj_set_style_border_color(dd, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_border_width(dd, 1, LV_PART_MAIN);
    lv_obj_set_style_text_font(dd, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(dd, lv_color_white(), LV_PART_MAIN);
    lv_dropdown_set_dir(dd, LV_DIR_TOP);
    // Prevent the parent container from scrolling to reveal this widget on focus/click.
    lv_obj_clear_flag(dd, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    // Prevent scroll events inside the list from propagating to the parent container.
    lv_obj_t *list = lv_dropdown_get_list(dd);
    if (list) {
        lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
        lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
    }
    return dd;
}

// Rebuilds s_channelDd option list from s_chanNames. Call from GUI task only.
static void rebuildChannelOptions() {
    if (!s_channelDd) return;
    char opts[GCM_MAX_CHANNELS * 20 + 1];
    size_t pos = 0;
    for (uint8_t i = 0; i < s_chanCount && pos < sizeof(opts) - 1; i++) {
        if (i > 0 && pos < sizeof(opts) - 1) opts[pos++] = '\n';
        int n;
        if (s_chanNames[i][0] != '\0')
            n = snprintf(opts + pos, sizeof(opts) - pos, "Ch %u: %s", (unsigned)i, s_chanNames[i]);
        else
            n = snprintf(opts + pos, sizeof(opts) - pos, "Ch %u", (unsigned)i);
        if (n > 0) pos += (size_t)n;
    }
    opts[pos] = '\0';
    lv_dropdown_set_options(s_channelDd, opts);
    if (s_channel >= s_chanCount) s_channel = 0;
    lv_dropdown_set_selected(s_channelDd, s_channel);
}

static void channelChangedCb(lv_event_t *e) {
    s_channel = (uint8_t)lv_dropdown_get_selected(s_channelDd);
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

    // Channel selector dropdown (NEW mode only).
    // Width 152 reserves right side for the future Destination dropdown.
    s_channelDd = makeSelectorDropdown(body, 0, 152);
    rebuildChannelOptions();
    lv_obj_add_event_cb(s_channelDd, channelChangedCb, LV_EVENT_VALUE_CHANGED, nullptr);

    // 8 slot buttons in 2 cols × 4 rows.
    // gridY0=70, rowH=28: grid bottom = 70+(4*28+3*4)=194px, body content=196px, 2px margin.
    const int gridX0 = 0;
    const int gridY0 = 70;
    const int colW   = 156;
    const int rowH   = 28;
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
    if (s_channelDd) {
        if (s_mode == HUB_MODE_NEW) {
            rebuildChannelOptions();
            lv_obj_clear_flag(s_channelDd, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_channelDd, LV_OBJ_FLAG_HIDDEN);
        }
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
