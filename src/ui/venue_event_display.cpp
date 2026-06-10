#include "venue_event_display.h"
#include "config.h"
#include "globals.h"
#include "ui_eez/screens.h"

// Static variables to track table state
static lv_obj_t* current_venue_table_container = nullptr;
static char last_displayed_data[HP_VENUE_DATA_SIZE];
static bool is_now_playing_screen_active = false;

void displayVenueEventTable(const char* dataString) {
    lv_obj_t * current_screen = lv_scr_act();

    // Clear existing table if it exists (for updates)
    if (current_venue_table_container != nullptr) {
        lv_obj_del(current_venue_table_container);
        current_venue_table_container = nullptr;
    }

    // Count events — count '#' delimiters, zero heap alloc
    int eventCount = 0;
    for (const char* p = dataString; *p; p++) {
        if (*p == '#') eventCount++;
    }

    int maxEvents = min(eventCount, 12);
    if (maxEvents == 0) maxEvents = 1;

    // Table directly on screen — no container wrapper. Skipping the intermediate lv_obj
    // eliminates one extra layer of LVGL draw overhead.
    lv_obj_t * table = lv_table_create(current_screen);
    if (table == nullptr) {
        Serial.println("[NOW_PLAYING] lv_table_create OOM, skipping");
        return;
    }
    current_venue_table_container = table;

    lv_table_set_col_cnt(table, 2);
    lv_table_set_row_cnt(table, maxEvents);

    lv_table_set_col_width(table, 0, (TFT_HEIGHT - 20) / 2);
    lv_table_set_col_width(table, 1, (TFT_HEIGHT - 20) / 2);

    lv_obj_set_pos(table, 0, 40);
    lv_obj_set_size(table, TFT_HEIGHT - 10, TFT_WIDTH - 50);  // 310×190

    // Style table — radius=0 prevents LVGL from creating an offline draw layer for
    // rounded-corner clipping. That layer consumed ~43KB during rendering (the full
    // largest-block), leaving insufficient heap for subsequent draw tasks.
    lv_obj_set_style_bg_color(table, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(table, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(table, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(table, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(table, false, LV_PART_MAIN);

    // Style cells
    lv_obj_set_style_bg_color(table, lv_color_hex(0x404040), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(table, lv_color_white(), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(table, &lv_font_montserrat_14, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(table, lv_color_hex(0x808080), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(table, 1, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(table, 3, LV_PART_ITEMS | LV_STATE_DEFAULT);

    // Style scrollbar
    lv_obj_set_style_width(table, 16, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(table, lv_color_hex(0x9e9e9e), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(table, 0, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);

    // Parse and populate — C-string pointer walk, zero heap alloc
    char venBuf[HP_VENUE_DATA_SIZE];
    char evBuf[HP_VENUE_DATA_SIZE];
    int row = 0;
    const char* p = dataString;

    while (*p && row < maxEvents) {
        const char* hash = strchr(p, '#');
        if (!hash) break;

        const char* comma = p;
        while (comma < hash && *comma != ',') comma++;

        if (comma < hash) {
            // copy + trim venue: p..comma-1
            const char* vs = p;
            const char* ve = comma;
            while (vs < ve && *vs == ' ') vs++;
            while (ve > vs && *(ve - 1) == ' ') ve--;
            int vLen = min((int)(ve - vs), (int)sizeof(venBuf) - 1);
            memcpy(venBuf, vs, vLen);
            venBuf[vLen] = '\0';

            // copy + trim event: comma+1..hash-1
            const char* es = comma + 1;
            const char* ee = hash;
            while (es < ee && *es == ' ') es++;
            while (ee > es && *(ee - 1) == ' ') ee--;
            int eLen = min((int)(ee - es), (int)sizeof(evBuf) - 1);
            memcpy(evBuf, es, eLen);
            evBuf[eLen] = '\0';

            lv_table_set_cell_value(table, row, 0, venBuf);
            lv_table_set_cell_value(table, row, 1, evBuf);
            row++;
        }
        p = hash + 1;
    }

    if (row == 0) {
        lv_table_set_cell_value(table, 0, 0, "No Data");
        lv_table_set_cell_value(table, 0, 1, "Available");
    }
    
    // Store what we just displayed
    strncpy(last_displayed_data, dataString, HP_VENUE_DATA_SIZE - 1);
    last_displayed_data[HP_VENUE_DATA_SIZE - 1] = '\0';

    Serial.printf("Table created with %d rows\n", maxEvents);
}

extern "C" void action_display_now_playing(lv_event_t *e) {
    // Mark that we're now on the Now Playing screen
    is_now_playing_screen_active = true;

    // Only show received data if GPS has synced at least once (lastGpsTimeUpdate != 0).
    // Without a GPS sync we cannot validate the date of received data.
    bool gpsHasSynced = (lastGpsTimeUpdate != 0);

    // Read from active buffer (no mutex needed - double buffering ensures lock-free reads)
    int activeBuffer = hotPacketActiveBufferNp;  // Snapshot current buffer
    if (gpsHasSynced && hotPacketBuffer_live_venue_event_data[activeBuffer][0] != '\0') {
        Serial.println(np_data_is_stored ? "Using stored venue/event data from EEPROM" : "Using live venue/event data from Meshtastic");
        displayVenueEventTable(hotPacketBuffer_live_venue_event_data[activeBuffer]);
    } else {
        Serial.println("No venue/event data available - showing NO DATA YET");
    }
}

void checkAndUpdateNowPlayingScreen() {
    // Only check if we're on the Now Playing screen and have new data
    if (!is_now_playing_screen_active || !new_rx_data_flag) {
        return;
    }

    // Critical fix: Verify we're actually on the Now Playing screen
    lv_obj_t* current_screen = lv_scr_act();
    if (current_screen != objects.now_playing) {
        // We're not on the Now Playing screen anymore, clean up state
        Serial.println("checkAndUpdateNowPlayingScreen: Not on Now Playing screen, calling onNowPlayingScreenExit");
        onNowPlayingScreenExit();
        return;
    }

    // Check if the data has actually changed
    const char* currentData = nullptr;

    bool gpsHasSynced = (lastGpsTimeUpdate != 0);

    // Read from active buffer (no mutex needed - double buffering ensures lock-free reads)
    int activeBuffer = hotPacketActiveBufferNp;  // Snapshot current buffer
    if (gpsHasSynced && hotPacketBuffer_live_venue_event_data[activeBuffer][0] != '\0') {
        currentData = hotPacketBuffer_live_venue_event_data[activeBuffer];
    }

    // Only refresh if data is available and has changed
    if (currentData && strcmp(last_displayed_data, currentData) != 0) {
        Serial.println("Now Playing screen: Refreshing with new data");
        displayVenueEventTable(currentData);
    }
}

void onNowPlayingScreenExit() {
    is_now_playing_screen_active = false;
    if (current_venue_table_container != nullptr) {
        lv_obj_del(current_venue_table_container);
        current_venue_table_container = nullptr;
    }
    last_displayed_data[0] = '\0';
}