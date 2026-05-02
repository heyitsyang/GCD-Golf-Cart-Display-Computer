#include "canned_screen.h"
#include "canned_replies.h"
#include "keyboard_bridge.h"
#include "communication/chat_buffer.h"
#include "config.h"
#include "globals.h"
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
static bool       s_editToggle = false;

// Hand-coded LVGL children of canned_msgs_container_body. The body is
// built lazily on first navigation to the canned hub (saves ~8 KB of
// largest contiguous block at boot, which WiFi pm_attach needs).
static lv_obj_t *s_contextStrip = nullptr;
static lv_obj_t *s_channelBtn   = nullptr;   // cycles 0→1→2 (NEW mode only)
static lv_obj_t *s_channelLabel = nullptr;
static lv_obj_t *s_editBtn      = nullptr;
static lv_obj_t *s_editLabel    = nullptr;
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
    cm.deleted   = false;
    strncpy(cm.text, text, sizeof(cm.text) - 1);
    cm.text[sizeof(cm.text) - 1] = '\0';
    chatBufferAppend(&cm);
}

static void chatSendCustomCb(const char *text, void *userdata) {
    if (!text || !*text) return;
    enqueueTx(s_channel, s_dest, text);

    // Append outgoing line to the running transcript and clear the typing line.
    String line = String("> ") + text + "\n";
    kb_set_context_append(line.c_str());
    if (objects.output_keyboard) {
        lv_textarea_set_text(objects.output_keyboard, "");
    }
}

static void slotClickedCb(lv_event_t *e) {
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
    uint8_t idx = (uint8_t)(uintptr_t)lv_obj_get_user_data(btn);
    if (idx >= CANNED_REPLY_COUNT) return;

    if (s_editToggle) {
        char ctx[32];
        snprintf(ctx, sizeof(ctx), "Edit slot %u", (unsigned)idx);
        kb_request_canned_edit(cannedReplyGet(idx), ctx,
                               saveCannedSlot, (void *)(uintptr_t)idx);
    } else {
        const char *text = cannedReplyGet(idx);
        enqueueTx(s_channel, s_dest, text);
        eez_flow_pop_screen(LV_SCR_LOAD_ANIM_NONE, 200, 0);
    }
}

static void editToggleCb(lv_event_t *e) {
    s_editToggle = !s_editToggle;
    if (s_editLabel) {
        lv_label_set_text(s_editLabel, s_editToggle ? "EDIT" : "Edit");
    }
}

static void channelCycleCb(lv_event_t *e) {
    s_channel = (s_channel + 1) % 3;  // 0..2 per plan §12 (Ch 0..2)
    if (s_channelLabel) {
        char buf[8];
        snprintf(buf, sizeof(buf), "Ch %u", (unsigned)s_channel);
        lv_label_set_text(s_channelLabel, buf);
    }
}

static void kbOpenClickedCb(lv_event_t *e) {
    // EEZ flow on btn_kb_open will push Keyboard_Entry next; arm
    // bridge state for sticky chat mode now so the prefill is in
    // place before the screen renders.
    kb_arm_chat("", s_context.c_str(), chatSendCustomCb, nullptr, nullptr);
}

static void buildBody() {
    if (s_bodyBuilt) return;
    if (!objects.canned_msgs_container_body) return;
    lv_obj_t *body = objects.canned_msgs_container_body;

    lv_obj_set_style_bg_color(body, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(body, 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(body, 0, LV_PART_MAIN);

    // Context strip — read-only textarea, top of body.
    s_contextStrip = lv_textarea_create(body);
    lv_obj_set_pos(s_contextStrip, 0, 0);
    lv_obj_set_size(s_contextStrip, 316, 38);
    lv_textarea_set_one_line(s_contextStrip, false);
    lv_textarea_set_text(s_contextStrip, "");
    lv_obj_add_state(s_contextStrip, LV_STATE_DISABLED);
    lv_obj_set_style_text_color(s_contextStrip, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_contextStrip, lv_color_hex(0x101010), LV_PART_MAIN);

    // Channel cycler (left) — visible only in NEW mode.
    s_channelBtn = lv_btn_create(body);
    lv_obj_set_pos(s_channelBtn, 0, 42);
    lv_obj_set_size(s_channelBtn, 70, 22);
    s_channelLabel = lv_label_create(s_channelBtn);
    lv_label_set_text(s_channelLabel, "Ch 0");
    lv_obj_center(s_channelLabel);
    lv_obj_add_event_cb(s_channelBtn, channelCycleCb, LV_EVENT_CLICKED, nullptr);

    // Edit toggle (right of channel cycler).
    s_editBtn = lv_btn_create(body);
    lv_obj_set_pos(s_editBtn, 246, 42);
    lv_obj_set_size(s_editBtn, 70, 22);
    s_editLabel = lv_label_create(s_editBtn);
    lv_label_set_text(s_editLabel, "Edit");
    lv_obj_center(s_editLabel);
    lv_obj_add_event_cb(s_editBtn, editToggleCb, LV_EVENT_CLICKED, nullptr);

    // 8 slot buttons in 2 cols × 4 rows. Body is 320 wide × 172 tall;
    // grid starts at y=68 so it sits above btn_kb_open at y≈177.
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
    // Build the canned hub body at boot (eager). An earlier lazy
    // version deferred this to cannedScreenPump() to avoid an
    // observed WiFi pm_attach crash, but that turned out to fragment
    // heap during the screen-transition render and trigger LVGL OOMs
    // on double-tap. Eager build matches the configuration that
    // worked end-to-end pre-experiment.
    buildBody();

    if (objects.btn_kb_open) {
        lv_obj_add_event_cb(objects.btn_kb_open, kbOpenClickedCb,
                            LV_EVENT_CLICKED, nullptr);
    }

    // Default to NEW mode until chat_screen tells us otherwise.
    cannedScreenSetNewMode();
}

static void applyModeUI() {
    if (s_contextStrip) {
        lv_textarea_set_text(s_contextStrip, s_context.c_str());
    }
    // Channel cycler is meaningful only for NEW mode (reply uses source ch).
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
    s_channel = 0;
    s_dest    = BROADCAST_ADDR;
    char buf[32];
    snprintf(buf, sizeof(buf), "New on Ch %u", (unsigned)s_channel);
    s_context = buf;
    s_uiDirty = true;
}

void cannedScreenPump() {
    if (lv_scr_act() != objects.meshtastic_canned_messages) return;

    if (!s_uiDirty) {
        // Still keep slot labels fresh in case NVS edits landed.
        for (uint8_t i = 0; i < CANNED_REPLY_COUNT; i++) {
            if (!s_slotLabels[i]) continue;
            const char *cur = lv_label_get_text(s_slotLabels[i]);
            const char *latest = cannedReplyGet(i);
            if (!cur || strcmp(cur, latest) != 0) {
                lv_label_set_text(s_slotLabels[i], latest);
            }
        }
        return;
    }
    s_uiDirty = false;

    applyModeUI();
    for (uint8_t i = 0; i < CANNED_REPLY_COUNT; i++) {
        if (s_slotLabels[i]) {
            lv_label_set_text(s_slotLabels[i], cannedReplyGet(i));
        }
    }
}
