#include "venue_event_display.h"
#include "config.h"
#include "globals.h"
#include "ui_eez/screens.h"

// Static variables to track table state
static lv_obj_t* current_venue_table_container = nullptr;
static String last_displayed_data = "";
static bool is_now_playing_screen_active = false;

void displayVenueEventTable(const char* dataString) {
    lv_obj_t * current_screen = lv_scr_act();

    // Clear existing table if it exists (for updates)
    if (current_venue_table_container != nullptr) {
        lv_obj_del(current_venue_table_container);
        current_venue_table_container = nullptr;
    }

    lv_obj_t * container = lv_obj_create(current_screen);
    current_venue_table_container = container; // Store reference for future updates
    lv_obj_set_pos(container, 0, 40);
    lv_obj_set_size(container, TFT_HEIGHT, TFT_WIDTH - 40);
    lv_obj_set_style_bg_color(container, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);

    // Disable container scrolling within the screen
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Count events
    String dataStr = String(dataString);
    int eventCount = 0;
    int pos = 0;
    
    while (pos < dataStr.length()) {
        int delimiterPos = dataStr.indexOf('#', pos);
        if (delimiterPos == -1) break;
        
        String pair = dataStr.substring(pos, delimiterPos);
        if (pair.indexOf(',') != -1) {
            eventCount++;
        }
        pos = delimiterPos + 1;
    }
    
    int maxEvents = min(eventCount, 12);
    if (maxEvents == 0) maxEvents = 1;
    
    // Create table
    lv_obj_t * table = lv_table_create(container);
    lv_table_set_col_cnt(table, 2);
    lv_table_set_row_cnt(table, maxEvents);
    
    lv_table_set_col_width(table, 0, (TFT_HEIGHT - 20) / 2);  // Width: use almost full 320px width
    lv_table_set_col_width(table, 1, (TFT_HEIGHT - 20) / 2);

    lv_obj_set_size(table, TFT_HEIGHT - 10, TFT_WIDTH - 50);  // Table: 310px wide, 190px tall
    lv_obj_align(table, LV_ALIGN_CENTER, 0, 0);
    
    // Style table
    lv_obj_set_style_bg_color(table, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(table, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(table, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(table, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // Style cells
    lv_obj_set_style_bg_color(table, lv_color_hex(0x404040), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(table, lv_color_white(), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(table, &lv_font_montserrat_18, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(table, lv_color_hex(0x808080), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(table, 1, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(table, 3, LV_PART_ITEMS | LV_STATE_DEFAULT);

    // Style scrollbar - make it twice as wide (default is typically 7-8px, so make it ~16px)
    lv_obj_set_style_width(table, 16, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(table, lv_color_hex(0x9e9e9e), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(table, 8, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    
    // Parse and populate
    int row = 0;
    int startPos = 0;
    
    while (startPos < dataStr.length() && row < maxEvents) {
        int delimiterPos = dataStr.indexOf('#', startPos);
        if (delimiterPos == -1) break;
        
        String pair = dataStr.substring(startPos, delimiterPos);
        int commaPos = pair.indexOf(',');
        
        if (commaPos != -1) {
            String venue = pair.substring(0, commaPos);
            String event = pair.substring(commaPos + 1);
            
            venue.trim();
            event.trim();
            
            lv_table_set_cell_value(table, row, 0, venue.c_str());
            lv_table_set_cell_value(table, row, 1, event.c_str());
            
            row++;
        }
        
        startPos = delimiterPos + 1;
    }
    
    if (row == 0) {
        lv_table_set_cell_value(table, 0, 0, "No Data");
        lv_table_set_cell_value(table, 0, 1, "Available");
    }
    
    // Store what we just displayed
    last_displayed_data = String(dataString);

    Serial.printf("Table created with %d rows\n", maxEvents);
}

extern "C" void action_display_now_playing(lv_event_t *e) {
    // Mark that we're now on the Now Playing screen
    is_now_playing_screen_active = true;

    const char* dataToDisplay;

    if (live_venue_event_data.length() > 0) {
        dataToDisplay = live_venue_event_data.c_str();
        Serial.println("Using live venue/event data from Meshtastic");
    } else {
        dataToDisplay = "Sawgrass,NA#Spanish Springs,NA#Lake Sumter,NA#Brownwood,NA#Sawgrass,NA#";
        Serial.println("Using default data - no live Meshtastic data available");
    }

    displayVenueEventTable(dataToDisplay);
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
    const char* currentData;
    if (live_venue_event_data.length() > 0) {
        currentData = live_venue_event_data.c_str();
    } else {
        currentData = "Sawgrass,NA#Spanish Springs,NA#Lake Sumter,NA#Brownwood,NA#Sawgrass,NA#";
    }

    // Only refresh if data has changed
    if (last_displayed_data != String(currentData)) {
        Serial.println("Now Playing screen: Refreshing with new data");
        displayVenueEventTable(currentData);
    }
}

void onNowPlayingScreenExit() {
    // Mark that we're no longer on the Now Playing screen
    is_now_playing_screen_active = false;

    // Clean up references
    current_venue_table_container = nullptr;
    last_displayed_data = "";
}