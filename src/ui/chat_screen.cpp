#include "chat_screen.h"
#include "canned_screen.h"
#include "communication/chat_buffer.h"
#include "config.h"
#include "globals.h"
#include "storage/preferences_manager.h"
#include "ui_eez/screens.h"
#include "ui_eez/eez-flow.h"
#include <Meshtastic.h>
#include <lvgl.h>
#include <time.h>

#define ROW_HEIGHT_PX        22
#define DOUBLE_TAP_WINDOW_MS 600
#define LONG_PRESS_MS        600

// Below this threshold, time(NULL) hasn't been seeded by GPS, so any
// formatted "time" would just be uptime since boot. ~July 2017.
#define TIMESTAMP_VALID_MIN  1500000000U

static volatile bool s_refreshPending = true;
static int32_t       s_lastBuiltFilter = -1;

// Selection / double-tap tracking
static lv_obj_t *s_selectedRow   = nullptr;
static lv_obj_t *s_lastTapTarget = nullptr;
static uint32_t  s_lastTapTime   = 0;

// Delete-confirm overlay (created once, shown/hidden as needed)
static lv_obj_t *s_deleteOverlay = nullptr;
static lv_obj_t *s_deletePreview = nullptr;
static uint32_t  s_pendingDeleteId = 0;

// Row pool — lazy-allocated on first Messages visit (not at boot).
// Boot-time pre-allocation consumed 16 KB which starved LVGL glyph rendering
// (~2.5 KB transient alloc) and caused abort() at ~41 s with no UI activity.
// LVGL v9 uses LV_STDLIB_CLIB: all objects come from system heap; rows are
// created once and reused (show/hide + label update) on every refresh.
static lv_obj_t *s_rows[CHAT_MAX_DISPLAY_ROWS];
static lv_obj_t *s_rowLabels[CHAT_MAX_DISPLAY_ROWS];
static bool      s_rowsInitialized = false;

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

// Direction is conveyed by row reverse-video styling, not by an
// arrow glyph. The first character of the text is the channel digit.
static void formatRowText(const chatMessage_t *m, char *out, size_t outSize) {
    char hhmm[10];
    buildTimeStr(m, hhmm, sizeof(hhmm));

    if (m->outgoing) {
        snprintf(out, outSize, "%u %s %s", (unsigned)m->channel, hhmm, m->text);
    } else {
        snprintf(out, outSize, "%u %s !%04x %s",
                 (unsigned)m->channel, hhmm,
                 (unsigned)(m->from & 0xFFFF), m->text);
    }
}

static void clearSelection() {
    if (s_selectedRow) {
        lv_obj_set_style_border_width(s_selectedRow, 0, LV_PART_MAIN);
        s_selectedRow = nullptr;
    }
}

static void applySelection(lv_obj_t *row) {
    clearSelection();
    s_selectedRow = row;
    lv_obj_set_style_border_color(row, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 2, LV_PART_MAIN);
}

static void openReplyForRow(lv_obj_t *row) {
    uint32_t id = (uint32_t)(uintptr_t)lv_obj_get_user_data(row);
    chatMessage_t m;
    if (!chatBufferGetById(id, &m)) return;

    // Keep the reply-context strip short: long strings make
    // lv_textarea_set_text request a larger realloc that often
    // fails in the fragmented runtime heap. Cap message preview
    // at 60 chars; the user already saw the full row text on the
    // chat list before tapping.
    char ctx[96];
    char sender[10];
    if (m.outgoing) {
        snprintf(sender, sizeof(sender), "ME");
    } else {
        snprintf(sender, sizeof(sender), "!%04x", (unsigned)(m.from & 0xFFFF));
    }
    snprintf(ctx, sizeof(ctx), "%s Ch%u: %.60s",
             sender, (unsigned)m.channel, m.text);

    // v1: replies are broadcast on the source channel (D5 deferred).
    cannedScreenSetReplyMode(m.channel, BROADCAST_ADDR, ctx);
    eez_flow_push_screen(SCREEN_ID_MESHTASTIC_CANNED_MESSAGES,
                         LV_SCR_LOAD_ANIM_NONE, 200, 0);
}

static void showDeleteOverlay(lv_obj_t *row) {
    if (!s_deleteOverlay) return;
    uint32_t id = (uint32_t)(uintptr_t)lv_obj_get_user_data(row);
    chatMessage_t m;
    if (!chatBufferGetById(id, &m)) return;

    s_pendingDeleteId = id;
    if (s_deletePreview) {
        char preview[128];
        snprintf(preview, sizeof(preview), "Delete:\n%.96s", m.text);
        lv_label_set_text(s_deletePreview, preview);
    }
    lv_obj_clear_flag(s_deleteOverlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_deleteOverlay);
}

static void rowClickedCb(lv_event_t *e) {
    lv_obj_t *target = (lv_obj_t *)lv_event_get_target(e);
    uint32_t now = millis();
    if (target == s_lastTapTarget && (now - s_lastTapTime) <= DOUBLE_TAP_WINDOW_MS) {
        s_lastTapTarget = nullptr;
        s_lastTapTime = 0;
        openReplyForRow(target);
    } else {
        s_lastTapTarget = target;
        s_lastTapTime = now;
        applySelection(target);
    }
}

static void rowLongPressedCb(lv_event_t *e) {
    lv_obj_t *target = (lv_obj_t *)lv_event_get_target(e);
    showDeleteOverlay(target);
}

static void deleteConfirmCb(lv_event_t *e) {
    if (s_pendingDeleteId != 0) {
        chatBufferDelete(s_pendingDeleteId);
        s_pendingDeleteId = 0;
    }
    if (s_deleteOverlay) lv_obj_add_flag(s_deleteOverlay, LV_OBJ_FLAG_HIDDEN);
    chatScreenRequestRefresh();
}

static void deleteCancelCb(lv_event_t *e) {
    s_pendingDeleteId = 0;
    if (s_deleteOverlay) lv_obj_add_flag(s_deleteOverlay, LV_OBJ_FLAG_HIDDEN);
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

static lv_obj_t *createDeleteOverlay(lv_obj_t *parent) {
    lv_obj_t *overlay = lv_obj_create(parent);
    lv_obj_set_size(overlay, 280, 120);
    lv_obj_align(overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x202020), LV_PART_MAIN);
    lv_obj_set_style_border_color(overlay, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_width(overlay, 2, LV_PART_MAIN);
    lv_obj_set_style_text_color(overlay, lv_color_white(), LV_PART_MAIN);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);

    s_deletePreview = lv_label_create(overlay);
    lv_label_set_long_mode(s_deletePreview, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_deletePreview, 260);
    lv_obj_align(s_deletePreview, LV_ALIGN_TOP_MID, 0, 4);
    lv_label_set_text(s_deletePreview, "");

    lv_obj_t *btnDel = lv_btn_create(overlay);
    lv_obj_set_size(btnDel, 110, 28);
    lv_obj_align(btnDel, LV_ALIGN_BOTTOM_LEFT, 4, -4);
    lv_obj_t *lblDel = lv_label_create(btnDel);
    lv_label_set_text(lblDel, "Delete");
    lv_obj_center(lblDel);
    lv_obj_add_event_cb(btnDel, deleteConfirmCb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *btnCxl = lv_btn_create(overlay);
    lv_obj_set_size(btnCxl, 110, 28);
    lv_obj_align(btnCxl, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
    lv_obj_t *lblCxl = lv_label_create(btnCxl);
    lv_label_set_text(lblCxl, "Cancel");
    lv_obj_center(lblCxl);
    lv_obj_add_event_cb(btnCxl, deleteCancelCb, LV_EVENT_CLICKED, nullptr);

    return overlay;
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
        lv_obj_set_style_radius(row, 0, LV_PART_MAIN);
        lv_obj_set_user_data(row, (void *)(uintptr_t)0);
        lv_obj_add_event_cb(row, rowClickedCb,     LV_EVENT_CLICKED,      nullptr);
        lv_obj_add_event_cb(row, rowLongPressedCb, LV_EVENT_LONG_PRESSED, nullptr);
        lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t *lbl = lv_label_create(row);
        lv_obj_set_style_text_color(lbl, lv_color_white(), LV_PART_MAIN);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl, lv_pct(100));
        lv_label_set_text(lbl, "");

        s_rows[i]      = row;
        s_rowLabels[i] = lbl;
    }
    s_rowsInitialized = true;
}

void chatScreenInit() {
    memset(s_rows,      0, sizeof(s_rows));
    memset(s_rowLabels, 0, sizeof(s_rowLabels));
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

    if (objects.meshtastic_messages) {
        s_deleteOverlay = createDeleteOverlay(objects.meshtastic_messages);
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
    s_lastTapTarget = nullptr;

    static chatMessage_t snapshot[CHAT_MAX_DISPLAY_ROWS];
    uint8_t filter = (uint8_t)((mesh_filter >= 0 && mesh_filter <= 4) ? mesh_filter : 0);
    size_t n = chatBufferSnapshot(filter, snapshot, CHAT_MAX_DISPLAY_ROWS);

    // Update pre-allocated rows in-place — no LVGL object creation or
    // destruction. LVGL v9 flex excludes hidden objects from layout.
    for (int i = 0; i < CHAT_MAX_DISPLAY_ROWS; i++) {
        if (!s_rows[i]) continue;
        if ((size_t)i < n) {
            const chatMessage_t *m = &snapshot[i];
            lv_obj_set_user_data(s_rows[i], (void *)(uintptr_t)m->id);

            lv_color_t row_bg   = m->outgoing ? lv_color_white() : lv_color_black();
            lv_color_t row_text = m->outgoing ? lv_color_black() : lv_color_white();
            lv_obj_set_style_bg_color(s_rows[i],      row_bg,   LV_PART_MAIN);
            lv_obj_set_style_text_color(s_rowLabels[i], row_text, LV_PART_MAIN);

            char rowText[CHAT_TEXT_SIZE + 64];
            formatRowText(m, rowText, sizeof(rowText));
            lv_label_set_text(s_rowLabels[i], rowText);

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
