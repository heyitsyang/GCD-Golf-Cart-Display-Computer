#include "hot_packet_parser.h"
#include "config.h"
#include "globals.h"
#include "types.h"
#include "utils/time_utils.h"

bool isHotPacket(const char* text) {
    return (text != NULL && text[0] == '|');
}

int parseHotPacketType(const char* text) {
    if (!isHotPacket(text)) return -1;
    
    // Check for valid format
    if (text[2] < '0' || text[2] > '9' || text[3] < '0' || text[3] > '9') {
        return -1;
    }
    
    // Convert to integer
    return (text[2] - '0') * 10 + (text[3] - '0');
}

void processHotPacket(const char* text) {
    Serial.println("Received a HoT pkt");

    // Set flag for any HOT packet received (for UI updates)
    new_rx_data_flag = true;

    int HotPktType = parseHotPacketType(text);

    if (HotPktType == -1) {
        Serial.println("Malformed HotPktType");
        return;
    }
    
    switch (HotPktType) {
        case HOT_PACKET_WEATHER: {
            Serial.println("WX packet received");

            // Parse weather data (updates buffers and swaps)
            // Pass the timestamp to be written to the same back buffer
            String timestamp = cur_date + "  " + hhmm_str + am_pm_str;
            if (parseWeatherData((char*)text, timestamp)) {
                // Update legacy variable for compatibility
                wx_rcv_time = hotPacketBuffer_wx_rcv_time[hotPacketActiveBuffer];
            }
            break;
        }

        case HOT_PACKET_VENUE_EVENT: {
            Serial.println("Venue/Event packet received");
            // Validate packet has enough data
            if (strlen(text) > HOT_PKT_HEADER_OFFSET) {
                // Write to back buffer (whichever is NOT active)
                int backBuffer = 1 - hotPacketActiveBuffer;
                hotPacketBuffer_np_rcv_time[backBuffer] = cur_date + "  " + hhmm_str + am_pm_str;
                hotPacketBuffer_live_venue_event_data[backBuffer] = String(&text[HOT_PKT_HEADER_OFFSET]);

                // Atomically swap buffers (very fast, no blocking for GUI)
                // Mutex only protects the pointer swap, not data reads
                bool haveMutex = (hotPacketMutex != NULL && xSemaphoreTake(hotPacketMutex, pdMS_TO_TICKS(10)) == pdTRUE);
                if (haveMutex || hotPacketMutex == NULL) {
                    hotPacketActiveBuffer = backBuffer;
                    if (haveMutex) xSemaphoreGive(hotPacketMutex);

                    // Update legacy variables for compatibility
                    np_rcv_time = hotPacketBuffer_np_rcv_time[backBuffer];
                    live_venue_event_data = hotPacketBuffer_live_venue_event_data[backBuffer];

                    Serial.print("Stored venue/event data: ");
                    Serial.println(live_venue_event_data);
                } else {
                    Serial.println("Buffer swap timeout (venue data)");
                }
            } else {
                Serial.println("Venue/Event packet too short");
            }
            break;
        }

        default:
            Serial.print("Unrecognized HotPktType: ");
            Serial.println(HotPktType);
            break;
    }
}

int parseWeatherData(char* input, const String& timestamp) {
    if (!input) return 0;

    int ptr, len;
    len = strlen(input);

    // Validate minimum packet size
    if (len < HOT_PKT_HEADER_OFFSET + 1) {
        Serial.println("Weather packet too short");
        return 0;
    }

    // Replace delimiters with null terminators
    for (ptr = 0; ptr < len; ptr++) {
        if ((input[ptr] == '#') || (input[ptr] == ',')) {
            input[ptr] = '\0';
        }
    }

    ptr = HOT_PKT_HEADER_OFFSET;

    // Write to back buffer (whichever is NOT active)
    int backBuffer = 1 - hotPacketActiveBuffer;

    // Write timestamp to back buffer
    hotPacketBuffer_wx_rcv_time[backBuffer] = timestamp;

    // Parse cur_temp to back buffer
    if (ptr >= len) return 0;
    hotPacketBuffer_cur_temp[backBuffer] = String(&input[ptr]);
    ptr = ptr + strlen(&input[ptr]) + 1;

    // Parse forecast hour 1
    if (ptr >= len) return 0;
    hotPacketBuffer_fcast_hr1[backBuffer] = String(&input[ptr]);
    ptr = ptr + hotPacketBuffer_fcast_hr1[backBuffer].length() + 1;
    if (ptr >= len) return 0;
    hotPacketBuffer_fcast_glyph1[backBuffer] = String(&input[ptr]);
    ptr = ptr + hotPacketBuffer_fcast_glyph1[backBuffer].length() + 1;
    if (ptr >= len) return 0;
    hotPacketBuffer_fcast_precip1[backBuffer] = String(&input[ptr]);
    int precip1_len = hotPacketBuffer_fcast_precip1[backBuffer].length();
    if (hotPacketBuffer_fcast_precip1[backBuffer] == "0.0") hotPacketBuffer_fcast_precip1[backBuffer] = "";
    ptr = ptr + precip1_len + 1;

    // Parse forecast hour 2
    if (ptr >= len) return 0;
    hotPacketBuffer_fcast_hr2[backBuffer] = String(&input[ptr]);
    ptr = ptr + hotPacketBuffer_fcast_hr2[backBuffer].length() + 1;
    if (ptr >= len) return 0;
    hotPacketBuffer_fcast_glyph2[backBuffer] = String(&input[ptr]);
    ptr = ptr + hotPacketBuffer_fcast_glyph2[backBuffer].length() + 1;
    if (ptr >= len) return 0;
    hotPacketBuffer_fcast_precip2[backBuffer] = String(&input[ptr]);
    int precip2_len = hotPacketBuffer_fcast_precip2[backBuffer].length();
    if (hotPacketBuffer_fcast_precip2[backBuffer] == "0.0") hotPacketBuffer_fcast_precip2[backBuffer] = "";
    ptr = ptr + precip2_len + 1;

    // Parse forecast hour 3
    if (ptr >= len) return 0;
    hotPacketBuffer_fcast_hr3[backBuffer] = String(&input[ptr]);
    ptr = ptr + hotPacketBuffer_fcast_hr3[backBuffer].length() + 1;
    if (ptr >= len) return 0;
    hotPacketBuffer_fcast_glyph3[backBuffer] = String(&input[ptr]);
    ptr = ptr + hotPacketBuffer_fcast_glyph3[backBuffer].length() + 1;
    if (ptr >= len) return 0;
    hotPacketBuffer_fcast_precip3[backBuffer] = String(&input[ptr]);
    int precip3_len = hotPacketBuffer_fcast_precip3[backBuffer].length();
    if (hotPacketBuffer_fcast_precip3[backBuffer] == "0.0") hotPacketBuffer_fcast_precip3[backBuffer] = "";
    ptr = ptr + precip3_len + 1;

    // Parse forecast hour 4
    if (ptr >= len) return 0;
    hotPacketBuffer_fcast_hr4[backBuffer] = String(&input[ptr]);
    ptr = ptr + hotPacketBuffer_fcast_hr4[backBuffer].length() + 1;
    if (ptr >= len) return 0;
    hotPacketBuffer_fcast_glyph4[backBuffer] = String(&input[ptr]);
    ptr = ptr + hotPacketBuffer_fcast_glyph4[backBuffer].length() + 1;
    if (ptr >= len) return 0;
    hotPacketBuffer_fcast_precip4[backBuffer] = String(&input[ptr]);
    int precip4_len = hotPacketBuffer_fcast_precip4[backBuffer].length();
    if (hotPacketBuffer_fcast_precip4[backBuffer] == "0.0") hotPacketBuffer_fcast_precip4[backBuffer] = "";
    ptr = ptr + precip4_len + 1;

    // Atomically swap buffers (very fast, no blocking for GUI)
    // Mutex only protects the pointer swap, not data reads (10ms timeout is very short)
    bool haveMutex = (hotPacketMutex != NULL && xSemaphoreTake(hotPacketMutex, pdMS_TO_TICKS(10)) == pdTRUE);
    if (haveMutex || hotPacketMutex == NULL) {
        hotPacketActiveBuffer = backBuffer;
        if (haveMutex) xSemaphoreGive(hotPacketMutex);

        // Update legacy variables for compatibility
        wx_rcv_time = hotPacketBuffer_wx_rcv_time[backBuffer];
        cur_temp = hotPacketBuffer_cur_temp[backBuffer];
        fcast_hr1 = hotPacketBuffer_fcast_hr1[backBuffer];
        fcast_glyph1 = hotPacketBuffer_fcast_glyph1[backBuffer];
        fcast_precip1 = hotPacketBuffer_fcast_precip1[backBuffer];
        fcast_hr2 = hotPacketBuffer_fcast_hr2[backBuffer];
        fcast_glyph2 = hotPacketBuffer_fcast_glyph2[backBuffer];
        fcast_precip2 = hotPacketBuffer_fcast_precip2[backBuffer];
        fcast_hr3 = hotPacketBuffer_fcast_hr3[backBuffer];
        fcast_glyph3 = hotPacketBuffer_fcast_glyph3[backBuffer];
        fcast_precip3 = hotPacketBuffer_fcast_precip3[backBuffer];
        fcast_hr4 = hotPacketBuffer_fcast_hr4[backBuffer];
        fcast_glyph4 = hotPacketBuffer_fcast_glyph4[backBuffer];
        fcast_precip4 = hotPacketBuffer_fcast_precip4[backBuffer];

        Serial.println("Weather data parsed successfully");
        return 1;
    } else {
        Serial.println("Buffer swap timeout (weather data)");
        return 0;
    }
}

void parseVenueEventData(const char* input) {
    // This function can be expanded to parse venue/event data into structured format
    // For now, the raw data is stored in live_venue_event_data
}