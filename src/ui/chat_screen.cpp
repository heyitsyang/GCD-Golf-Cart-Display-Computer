#include "chat_screen.h"
#include "canned_screen.h"
#include "communication/chat_buffer.h"
#include "communication/meshtastic_admin.h"
#include "config.h"
#include "globals.h"
#include "storage/favorites.h"
#include "storage/preferences_manager.h"
#include "ui_eez/screens.h"
#include "ui_eez/eez-flow.h"
#include <Meshtastic.h>
#include <lvgl.h>
#include <time.h>

#define ROW_HEIGHT_PX   32
#define PREFIX_WIDTH_PX 52

// Below this threshold, time(NULL) hasn't been seeded by GPS, so any
// formatted "time" would just be uptime since boot. ~July 2017.
#define TIMESTAMP_VALID_MIN  1500000000U

static volatile bool s_refreshPending = true;
static int32_t       s_lastBuiltFilter = -1;

static lv_obj_t *s_selectedRow = nullptr;

// Row pool — lazy-allocated on first Messages visit (not at boot).
// Boot-time pre-allocation consumed 16 KB which starved LVGL glyph rendering
// (~2.5 KB transient alloc) and caused abort() at ~41 s with no UI activity.
// LVGL v9 uses LV_STDLIB_CLIB: all objects come from system heap; rows are
// created once and reused (show/hide + label update) on every refresh.
static lv_obj_t *s_rows[CHAT_MAX_DISPLAY_ROWS];
static lv_obj_t *s_rowPrefixLabels[CHAT_MAX_DISPLAY_ROWS];
static lv_obj_t *s_rowLabels[CHAT_MAX_DISPLAY_ROWS];
static bool      s_rowsInitialized = false;

// Snapshot of the last chatBufferSnapshot() call — module-scope so that
// clearSelection() can re-truncate after the marquee restores full text.
static chatMessage_t s_snapshot[CHAT_MAX_DISPLAY_ROWS];
static size_t        s_snapshotCount = 0;

// 12-hour HH:MMa/p, lowercase. "--:--" if timestamp not synced (GPS
// hasn't seeded the RTC yet).
static void buildTimeStr(const chatMessage_t *m, char *out, size_t outSize) {
    if (m->timestamp < TIMESTAMP_VALID_MIN) {
        snprintf(out, outSize, "--:--");
        return;
    }
    time_t t = (time_t)m->timestamp;
    struct tm tmv;
    if (!localtime_r(&t, &tmv)) {
        snprintf(out, outSize, "--:--");
        return;
    }
    int h = tmv.tm_hour;
    char ap = (h < 12) ? 'a' : 'p';
    int h12 = h % 12;
    if (h12 == 0) h12 = 12;
    snprintf(out, outSize, "%d:%02d%c", h12, tmv.tm_min, ap);
}

// Prefix: channel + time on line 1, address on line 2 (in smaller font).
// Direction is conveyed by row reverse-video styling.
static void buildPrefixStr(const chatMessage_t *m, char *out, size_t outSize) {
    char hhmm[10];
    buildTimeStr(m, hhmm, sizeof(hhmm));
    uint32_t addr = m->outgoing ? m->to : m->from;
    snprintf(out, outSize, "%u %s\n!%04x",
             (unsigned)m->channel, hhmm,
             (unsigned)(addr & 0xFFFF));
}

static int rowIndexFor(lv_obj_t *row) {
    for (int i = 0; i < CHAT_MAX_DISPLAY_ROWS; i++) {
        if (s_rows[i] == row) return i;
    }
    return -1;
}

// Set the message label for row i.  In marquee mode the full text is loaded
// for SCROLL_CIRCULAR.  Otherwise we measure the pixel width and append ">"
// at the exact cutoff when the text overflows the available label width.
static void applyMsgText(int i, bool marquee) {
    lv_obj_t *lbl = s_rowLabels[i];
    if (!lbl || (size_t)i >= s_snapshotCount) return;
    const char *text = s_snapshot[i].text;

    if (marquee) {
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_label_set_text(lbl, text);
        return;
    }

    // Available pixel width: use actual content width after first layout;
    // fall back to a calculated constant before the first render pass.
    // Row padding: pad_all=1 each side, pad_column=2 between prefix and label.
    // Label width ≈ body_width − PREFIX_WIDTH_PX − 2×1 − 2 = body_width − 56.
    // For the 320 px CYD screen the body fills the screen → ~264 px.
    int32_t avail = lv_obj_get_content_width(lbl);
    if (avail <= 0) avail = 264;

    const lv_font_t *font = lv_obj_get_style_text_font(lbl, LV_PART_MAIN);
    int32_t          ls   = lv_obj_get_style_text_letter_space(lbl, LV_PART_MAIN);

    size_t  len    = strlen(text);
    int32_t full_w = lv_text_get_width(text, (uint32_t)len, font, ls);

    if (full_w <= avail) {
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        lv_label_set_text(lbl, text);
        return;
    }

    // Overflow: binary-search for the longest byte prefix that fits with ">".
    int32_t suffix_w = lv_text_get_width(">", 1, font, ls);
    int32_t budget   = avail - suffix_w;

    size_t lo = 0, hi = len;
    while (lo < hi) {
        size_t  mid = (lo + hi + 1) / 2;
        int32_t w   = lv_text_get_width(text, (uint32_t)mid, font, ls);
        if (w <= budget) lo = mid;
        else             hi = mid - 1;
    }

    char buf[CHAT_TEXT_SIZE + 2];
    memcpy(buf, text, lo);
    buf[lo]     = '>';
    buf[lo + 1] = '\0';

    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_label_set_text(lbl, buf);
}

static void clearSelection() {
    if (s_selectedRow) {
        lv_obj_set_style_border_width(s_selectedRow, 0, LV_PART_MAIN);
        int idx = rowIndexFor(s_selectedRow);
        if (idx >= 0) applyMsgText(idx, false);
        s_selectedRow = nullptr;
    }
}

static void applySelection(lv_obj_t *row) {
    clearSelection();
    s_selectedRow = row;
    lv_obj_set_style_border_color(row, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 2, LV_PART_MAIN);
    int idx = rowIndexFor(row);
    if (idx >= 0) applyMsgText(idx, true);
}

static void openReplyForRow(lv_obj_t *row) {
    uint32_t id = (uint32_t)(uintptr_t)lv_obj_get_user_data(row);
    chatMessage_t m;
    if (!chatBufferGetById(id, &m)) return;

    // Mirror the reply mode of the source message: DM replies go back
    // to the originating node; channel messages broadcast on that channel.
    uint32_t replyDest;
    if (!m.outgoing && m.to == my_node_num) {
        replyDest = m.from;
    } else if (m.outgoing && m.to != BROADCAST_ADDR) {
        replyDest = m.to;
    } else {
        replyDest = BROADCAST_ADDR;
    }

    cannedScreenSetReplyMode(m.channel, replyDest, &m);
    eez_flow_push_screen(SCREEN_ID_MESHTASTIC_CANNED_MESSAGES,
                         LV_SCR_LOAD_ANIM_NONE, 200, 0);
}

// Left zone (prefix label): tap toggles favorite for incoming message sender.
static void rowLeftClickedCb(lv_event_t *e) {
    lv_obj_t *prefix = (lv_obj_t *)lv_event_get_current_target(e);
    lv_obj_t *row    = lv_obj_get_parent(prefix);
    uint32_t  id     = (uint32_t)(uintptr_t)lv_obj_get_user_data(row);

    chatMessage_t m;
    if (!chatBufferGetById(id, &m) || m.outgoing) {
        lv_event_stop_bubbling(e);
        return;
    }
    uint32_t nodeId = m.from;
    if (nodeId == 0 || nodeId == BROADCAST_ADDR) {
        lv_event_stop_bubbling(e);
        return;
    }

    bool nowFav;
    if (favoritesContains(nodeId)) {
        favoritesRemove(nodeId);
        nowFav = false;
    } else {
        const char* nm = nodeNameCacheLookup(nodeId);
        if (!favoritesAdd(nodeId, nm)) {
            lv_event_stop_bubbling(e);
            return;
        }
        // Name not yet known — request a full node list refresh from the Meshtastic task
        // so the name gets populated once the GCM responds.
        if (!nm || !nm[0]) nodeListRefreshRequested = true;
        nowFav = true;
    }
    lv_color_t color = nowFav ? lv_color_hex(0xFFFF00) : lv_color_white();
    lv_obj_set_style_text_color(prefix, color, LV_PART_MAIN);
    lv_event_stop_bubbling(e);
}

// Right zone (message label): tap selects row and starts marquee.
// Also marks the message read and immediately updates the text color.
static void rowRightClickedCb(lv_event_t *e) {
    lv_obj_t *lbl = (lv_obj_t *)lv_event_get_current_target(e);
    lv_obj_t *row = lv_obj_get_parent(lbl);
    uint32_t  id  = (uint32_t)(uintptr_t)lv_obj_get_user_data(row);
    chatBufferMarkRead(id);
    eepromWriteItem_t nvsItem = {};
    nvsItem.type = EEPROM_SAVE_DMS;
    xQueueSend(eepromWriteQueue, &nvsItem, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), LV_PART_MAIN);
    applySelection(row);
    lv_event_stop_bubbling(e);
}

// Right zone (message label): long press navigates to Canned Messages (reply).
// Marks the message read before navigating; Messages screen refreshes on return.
static void rowLongPressedCb(lv_event_t *e) {
    lv_obj_t *lbl = (lv_obj_t *)lv_event_get_current_target(e);
    lv_obj_t *row = lv_obj_get_parent(lbl);
    uint32_t  id  = (uint32_t)(uintptr_t)lv_obj_get_user_data(row);
    chatBufferMarkRead(id);
    eepromWriteItem_t nvsItem = {};
    nvsItem.type = EEPROM_SAVE_DMS;
    xQueueSend(eepromWriteQueue, &nvsItem, 0);
    openReplyForRow(row);
    lv_event_stop_bubbling(e);
}

static void filterChangedCb(lv_event_t *e) {
    if (!objects.filter_dropdown) return;
    int32_t idx = lv_dropdown_get_selected(objects.filter_dropdown);
    if (idx != mesh_filter) {
        mesh_filter = idx;
        queuePreferenceWrite("mesh_filter", (int)idx);
        chatScreenRequestRefresh();
    }
}

static void newComposeClickedCb(lv_event_t *e) {
    cannedScreenSetNewMode();
}

// Allocates the row pool on first entry to the Messages screen.
// Called from chatScreenRefresh() — runs in GUI task, no mutex needed.
static void ensureRowsAllocated() {
    if (s_rowsInitialized) return;
    if (!objects.messages_container_body) return;

    lv_obj_t *body = objects.messages_container_body;
    for (int i = 0; i < CHAT_MAX_DISPLAY_ROWS; i++) {
        lv_obj_t *row = lv_btn_create(body);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, ROW_HEIGHT_PX);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 1, LV_PART_MAIN);
        lv_obj_set_style_pad_column(row, 2, LV_PART_MAIN);
        lv_obj_set_style_radius(row, 0, LV_PART_MAIN);
        lv_obj_set_user_data(row, (void *)(uintptr_t)0);
        lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

        // Prefix: "ch time\n!addr" in small font — channel, stacked time+address.
        // Tap zone: toggles favorite for incoming message sender (yellow = favorited).
        lv_obj_t *prefix = lv_label_create(row);
        lv_obj_set_width(prefix, PREFIX_WIDTH_PX);
        lv_obj_set_height(prefix, LV_SIZE_CONTENT);
        lv_obj_set_style_text_font(prefix, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(prefix, lv_color_white(), LV_PART_MAIN);
        lv_label_set_long_mode(prefix, LV_LABEL_LONG_CLIP);
        lv_label_set_text(prefix, "");
        lv_obj_add_flag(prefix, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(prefix, rowLeftClickedCb, LV_EVENT_CLICKED, nullptr);

        // Message text — grows to fill remaining row width.
        // Tap: select + marquee. Long press: open reply screen.
        lv_obj_t *lbl = lv_label_create(row);
        lv_obj_set_flex_grow(lbl, 1);
        lv_obj_set_height(lbl, LV_SIZE_CONTENT);
        lv_obj_set_style_text_color(lbl, lv_color_white(), LV_PART_MAIN);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        lv_label_set_text(lbl, "");
        lv_obj_add_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(lbl, rowRightClickedCb, LV_EVENT_CLICKED, nullptr);
        lv_obj_add_event_cb(lbl, rowLongPressedCb, LV_EVENT_LONG_PRESSED, nullptr);

        s_rows[i]            = row;
        s_rowPrefixLabels[i] = prefix;
        s_rowLabels[i]       = lbl;
    }
    s_rowsInitialized = true;
}

void chatScreenInit() {
    memset(s_rows,            0, sizeof(s_rows));
    memset(s_rowPrefixLabels, 0, sizeof(s_rowPrefixLabels));
    memset(s_rowLabels,       0, sizeof(s_rowLabels));
    s_rowsInitialized = false;

    if (objects.messages_container_body) {
        lv_obj_t *body = objects.messages_container_body;
        lv_obj_set_style_bg_color(body, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_pad_all(body, 0, LV_PART_MAIN);
        lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_scroll_dir(body, LV_DIR_VER);
        lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
        // Rows are lazy-allocated on first Messages visit — see ensureRowsAllocated().
    }

    if (objects.filter_dropdown) {
        // Apply the persisted selection (loadPreferences set mesh_filter).
        if (mesh_filter >= 0 && mesh_filter <= 4) {
            lv_dropdown_set_selected(objects.filter_dropdown, mesh_filter);
        }
        lv_obj_add_event_cb(objects.filter_dropdown, filterChangedCb,
                            LV_EVENT_VALUE_CHANGED, nullptr);
    }

    if (objects.btn_new_msg) {
        lv_obj_add_event_cb(objects.btn_new_msg, newComposeClickedCb,
                            LV_EVENT_CLICKED, nullptr);
    }

}

void chatScreenRefresh() {
    if (!objects.messages_container_body) return;
    ensureRowsAllocated();

    // lv_mem_monitor() is a no-op with LV_STDLIB_CLIB (LVGL v9 uses system
    // malloc). Printed after ensureRowsAllocated() so it shows post-allocation
    // state — the actual headroom available before LVGL renders.
#if DEBUG_HEAP
    Serial.printf("[CHAT] refresh: sys free=%u max_alloc=%u\n",
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getMaxAllocHeap());
#endif

    lv_obj_t *body = objects.messages_container_body;

    clearSelection();

    uint8_t filter = (uint8_t)((mesh_filter >= 0 && mesh_filter <= 4) ? mesh_filter : 0);
    s_snapshotCount = chatBufferSnapshot(filter, s_snapshot, CHAT_MAX_DISPLAY_ROWS);

    // Update pre-allocated rows in-place — no LVGL object creation or
    // destruction. LVGL v9 flex excludes hidden objects from layout.
    for (int i = 0; i < CHAT_MAX_DISPLAY_ROWS; i++) {
        if (!s_rows[i]) continue;
        if ((size_t)i < s_snapshotCount) {
            const chatMessage_t *m = &s_snapshot[i];
            lv_obj_set_user_data(s_rows[i], (void *)(uintptr_t)m->id);

            lv_color_t row_bg  = m->outgoing ? lv_color_white() : lv_color_black();
            lv_color_t msg_col;
            if (m->outgoing) {
                msg_col = lv_color_black();
            } else if (!m->read && m->to == my_node_num) {
                msg_col = lv_color_hex(0x00FFFF);  // cyan = unread DM
            } else {
                msg_col = lv_color_white();
            }
            lv_color_t pfx_col;
            if (m->outgoing) {
                pfx_col = lv_color_black();
            } else {
                pfx_col = favoritesContains(m->from)
                              ? lv_color_hex(0xFFFF00)
                              : lv_color_white();
            }
            lv_obj_set_style_bg_color(s_rows[i], row_bg, LV_PART_MAIN);
            lv_obj_set_style_text_color(s_rowPrefixLabels[i], pfx_col, LV_PART_MAIN);
            lv_obj_set_style_text_color(s_rowLabels[i],       msg_col, LV_PART_MAIN);

            char prefix[24];
            buildPrefixStr(m, prefix, sizeof(prefix));
            lv_label_set_text(s_rowPrefixLabels[i], prefix);
            applyMsgText(i, false);

            lv_obj_clear_flag(s_rows[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_rows[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_obj_scroll_to_y(body, LV_COORD_MAX, LV_ANIM_OFF);
    s_lastBuiltFilter = mesh_filter;
}

void chatScreenRequestRefresh() {
    s_refreshPending = true;
}

void chatScreenPump() {
    if (lv_scr_act() != objects.meshtastic_messages) return;
    if (s_refreshPending || s_lastBuiltFilter != mesh_filter) {
        s_refreshPending = false;
        chatScreenRefresh();
    }
}
