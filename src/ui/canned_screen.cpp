#include "canned_screen.h"
#include "canned_replies.h"
#include "communication/chat_buffer.h"
#include "communication/meshtastic_admin.h"
#include "config.h"
#include "globals.h"
#include "storage/favorites.h"
#include "ui/chat_screen.h"
#include "ui_eez/screens.h"
#include "ui_eez/eez-flow.h"
#include <Meshtastic.h>
#include <lvgl.h>
#include <time.h>

typedef enum { HUB_MODE_REPLY, HUB_MODE_NEW } hub_mode_t;

static hub_mode_t  s_mode    = HUB_MODE_NEW;
static uint8_t     s_channel = 0;
static uint32_t    s_dest    = BROADCAST_ADDR;

// Cycle-button selectors replace lv_dropdown to eliminate ARGB8888 corner-layer
// allocations that occurred when opening a dropdown popup on a low-heap ESP32.
static lv_obj_t *s_channelBtn = nullptr;
static lv_obj_t *s_channelLbl = nullptr;
static lv_obj_t *s_recipientBtn = nullptr;
static lv_obj_t *s_recipientLbl = nullptr;
static uint8_t   s_recipientIdx = 0;

// Maps cycle-button index → node ID (BROADCAST_ADDR for the "Broadcast" entry).
// Rebuilt by rebuildDestOptions(); read by recipientBtnClickedCb().
static uint32_t s_destNodes[MAX_FAVORITES + 2];
static uint8_t  s_destNodeCount = 0;

#define GCM_MAX_CHANNELS 8
static char    s_chanNames[GCM_MAX_CHANNELS][12] = {};
static uint8_t s_chanCount = 3;
static lv_obj_t *s_slotBtns[CANNED_REPLY_COUNT]   = {nullptr};
static lv_obj_t *s_slotLabels[CANNED_REPLY_COUNT] = {nullptr};
static lv_obj_t *s_contextLbl  = nullptr;
static char      s_contextText[121] = {};

// Persistent label-string buffer for destination entries (newline-separated).
// rebuildDestOptions() writes it; getLabelFromOpts() reads individual lines.
// lv_label_set_text() copies the string, so no static-pointer requirement here.
static char s_destOptsStatic[(MAX_FAVORITES + 2) * 12] = {};

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

// Extracts the Nth newline-delimited line from opts into out[outsz].
static void getLabelFromOpts(const char *opts, uint8_t idx, char *out, size_t outsz) {
    const char *p = opts;
    for (uint8_t i = 0; i < idx; i++) {
        p = strchr(p, '\n');
        if (!p) { out[0] = '\0'; return; }
        p++;
    }
    const char *end = strchr(p, '\n');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    if (len >= outsz) len = outsz - 1;
    memcpy(out, p, len);
    out[len] = '\0';
}

// Creates a tap-to-cycle button with no popup list.
// radius=0 prevents the lv_refr rounded-corner ARGB8888 layer allocation path.
// Returns the button; sets *lblOut to the label child (or nullptr on OOM).
static lv_obj_t *makeCycleBtn(lv_obj_t *parent, int x, int w, int y, lv_obj_t **lblOut) {
    lv_obj_t *btn = lv_btn_create(parent);
    if (!btn) { if (lblOut) *lblOut = nullptr; return nullptr; }
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, 28);
    lv_obj_set_style_pad_all(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x003c6b), LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 0, LV_PART_MAIN);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl, w - 8);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(lbl, "");
    if (lblOut) *lblOut = lbl;
    return btn;
}

// ---- channel cycle button ---------------------------------------------------

static void updateChannelBtnLabel() {
    if (!s_channelLbl) return;
    char buf[20];
    if (s_chanNames[s_channel][0] != '\0')
        snprintf(buf, sizeof(buf), "Ch %u: %s", (unsigned)s_channel, s_chanNames[s_channel]);
    else
        snprintf(buf, sizeof(buf), "Ch %u", (unsigned)s_channel);
    lv_label_set_text(s_channelLbl, buf);
}

static void channelBtnClickedCb(lv_event_t *e) {
    s_channel = (s_channel + 1) % s_chanCount;
    updateChannelBtnLabel();
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

// ---- recipient cycle button -------------------------------------------------

// Appends one node entry to opts[]. Separator '\n' is prepended when pos > 0.
static void appendDestEntry(char* opts, size_t optsz, size_t& pos,
                             uint32_t* nodes, uint8_t& count,
                             uint32_t nodeId, const char* name) {
    if (pos > 0 && pos < optsz - 1) opts[pos++] = '\n';
    unsigned short4 = (unsigned)(nodeId & 0xFFFF);
    int n;
    if (name && name[0])
        n = snprintf(opts + pos, optsz - pos, "!%04x %s", short4, name);
    else
        n = snprintf(opts + pos, optsz - pos, "!%04x", short4);
    if (n > 0) pos += (size_t)n;
    nodes[count++] = nodeId;
}

// Rebuilds s_destNodes[] and s_destOptsStatic, pre-selects s_recipientIdx
// to match s_dest, then updates the recipient button label.
// Pass tempDest=0 when there is no temporary entry (NEW mode or reply to favorite/bcast).
static void rebuildDestOptions(uint32_t tempDest) {
    size_t pos = 0;
    s_destNodeCount = 0;

    if (tempDest != 0 && tempDest != BROADCAST_ADDR && !favoritesContains(tempDest))
        appendDestEntry(s_destOptsStatic, sizeof(s_destOptsStatic), pos, s_destNodes, s_destNodeCount,
                        tempDest, nodeNameCacheLookup(tempDest));

    for (uint8_t i = 0; i < favoritesCount(); i++) {
        uint32_t nid = favoritesGet(i);
        const char* nm = favoritesGetName(i);
        if (!nm || !nm[0]) {
            // Name was empty at favorite-add time; try the live cache now.
            const char* cached = nodeNameCacheLookup(nid);
#if DEBUG_GCM_MESSAGES
            Serial.printf("[CANNED] fav[%u] nid=%08x stored=\"%s\" cache=\"%s\"\n",
                          (unsigned)i, (unsigned)nid, nm ? nm : "(null)", cached ? cached : "(null)");
#endif
            if (cached && cached[0]) {
                favoritesUpdateName(nid, cached);  // persist lazily from GUI task
                nm = cached;
            }
        }
        appendDestEntry(s_destOptsStatic, sizeof(s_destOptsStatic), pos, s_destNodes, s_destNodeCount, nid, nm);
    }

    if (pos > 0 && pos < sizeof(s_destOptsStatic) - 1) s_destOptsStatic[pos++] = '\n';
    int n = snprintf(s_destOptsStatic + pos, sizeof(s_destOptsStatic) - pos, "Broadcast");
    if (n > 0) pos += (size_t)n;
    s_destNodes[s_destNodeCount++] = BROADCAST_ADDR;
    s_destOptsStatic[pos] = '\0';

    s_recipientIdx = (uint8_t)(s_destNodeCount - 1); // default Broadcast
    for (uint8_t i = 0; i < s_destNodeCount; i++) {
        if (s_destNodes[i] == s_dest) { s_recipientIdx = i; break; }
    }

    if (s_recipientLbl) {
        char line[20];
        getLabelFromOpts(s_destOptsStatic, s_recipientIdx, line, sizeof(line));
        lv_label_set_text(s_recipientLbl, line);
    }
}

static void recipientBtnClickedCb(lv_event_t *e) {
    if (s_destNodeCount == 0) return;
    s_recipientIdx = (s_recipientIdx + 1) % s_destNodeCount;
    s_dest = s_destNodes[s_recipientIdx];
    if (s_recipientLbl) {
        char line[20];
        getLabelFromOpts(s_destOptsStatic, s_recipientIdx, line, sizeof(line));
        lv_label_set_text(s_recipientLbl, line);
    }
}

// ---- build / lifecycle ------------------------------------------------------

static void buildBody() {
    if (s_bodyBuilt) return;
    if (!objects.canned_msgs_container_body) return;
    lv_obj_t *body = objects.canned_msgs_container_body;

    lv_obj_set_style_bg_color(body, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(body, 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(body, 0, LV_PART_MAIN);

    // Channel cycle-button (left half of selector row).
    // Recipient cycle-button is created lazily in applyModeUI() on first visit
    // to preserve boot-time heap for LVGL draw-layer allocations.
    s_channelBtn = makeCycleBtn(body, 0, 152, 2, &s_channelLbl);
    if (s_channelBtn)
        lv_obj_add_event_cb(s_channelBtn, channelBtnClickedCb, LV_EVENT_CLICKED, nullptr);
    updateChannelBtnLabel();

    // Context strip: shown in REPLY mode (white label with replied-to message text),
    // hidden in NEW mode (no frame — just background shows through).
    s_contextLbl = lv_label_create(body);
    if (s_contextLbl) {
        lv_obj_set_pos(s_contextLbl, 2, 34);
        lv_obj_set_size(s_contextLbl, 312, 34);
        lv_obj_set_style_bg_color(s_contextLbl, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_contextLbl, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_contextLbl, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_text_font(s_contextLbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(s_contextLbl, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_ver(s_contextLbl, 4, LV_PART_MAIN);
        lv_obj_set_style_radius(s_contextLbl, 0, LV_PART_MAIN);
        lv_label_set_long_mode(s_contextLbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_clear_flag(s_contextLbl, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
        lv_obj_add_flag(s_contextLbl, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_contextLbl, "");
    }

    // 8 slot buttons in 2 cols × 4 rows.
    // gridY0=70, rowH=28: grid bottom = 70+(4*28+3*4)=194px, body content=196px.
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
    lv_obj_t *body = objects.canned_msgs_container_body;
    if (!body) return;

    // Lazy-create recipient cycle-button on first canned screen visit.
    if (!s_recipientBtn) {
#if DEBUG_HEAP
        Serial.printf("[CANNED] pre-rcpt: sys free=%u max_alloc=%u\n",
                      (unsigned)ESP.getFreeHeap(),
                      (unsigned)ESP.getMaxAllocHeap());
#endif
        s_recipientBtn = makeCycleBtn(body, 156, 160, 2, &s_recipientLbl);
#if DEBUG_HEAP
        Serial.printf("[CANNED] post-rcpt: sys free=%u max_alloc=%u btn=%s\n",
                      (unsigned)ESP.getFreeHeap(),
                      (unsigned)ESP.getMaxAllocHeap(),
                      s_recipientBtn ? "OK" : "NULL");
#endif
        if (s_recipientBtn)
            lv_obj_add_event_cb(s_recipientBtn, recipientBtnClickedCb, LV_EVENT_CLICKED, nullptr);
    }

    if (s_contextLbl) {
        if (s_mode == HUB_MODE_REPLY && s_contextText[0]) {
            lv_label_set_text(s_contextLbl, s_contextText);
            lv_obj_clear_flag(s_contextLbl, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_contextLbl, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_channelBtn) {
        updateChannelBtnLabel();
        lv_obj_clear_flag(s_channelBtn, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_recipientBtn) {
        uint32_t tempDest = (s_mode == HUB_MODE_REPLY &&
                             s_dest != BROADCAST_ADDR &&
                             !favoritesContains(s_dest)) ? s_dest : 0;
        rebuildDestOptions(tempDest);
        lv_obj_clear_flag(s_recipientBtn, LV_OBJ_FLAG_HIDDEN);
    }
}

void cannedScreenSetReplyMode(uint8_t channel, uint32_t dest, const chatMessage_t *srcMsg) {
    s_mode    = HUB_MODE_REPLY;
    s_channel = channel;
    s_dest    = dest;
    if (srcMsg && srcMsg->text[0])
        strncpy(s_contextText, srcMsg->text, sizeof(s_contextText) - 1);
    else
        s_contextText[0] = '\0';
    s_contextText[sizeof(s_contextText) - 1] = '\0';
    s_uiDirty = true;
}

void cannedScreenSetNewMode() {
    s_mode           = HUB_MODE_NEW;
    s_dest           = BROADCAST_ADDR;
    s_contextText[0] = '\0';
    s_uiDirty        = true;
}

void cannedScreenPump() {
    if (lv_scr_act() != objects.meshtastic_canned_messages) return;
    if (!s_uiDirty) return;
    s_uiDirty = false;
    applyModeUI();
}

void cannedScreenFreeRecipientBtn() {
    if (!s_recipientBtn) return;
    lv_obj_del(s_recipientBtn);
    s_recipientBtn = nullptr;
    s_recipientLbl = nullptr;
    s_uiDirty = true;
}
