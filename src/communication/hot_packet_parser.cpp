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

            // Protect access to GPS-updated global strings (cur_date, hhmm_str, am_pm_str)
            String timestamp;
            if (gpsMutex != NULL && xSemaphoreTake(gpsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                timestamp = cur_date + "  " + hhmm_str + am_pm_str;
                xSemaphoreGive(gpsMutex);
            } else {
                timestamp = "GPS data unavailable";
            }

            // Parse weather data (updates buffers and swaps)
            // Pass the timestamp to be written to the same back buffer
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

                // Protect access to GPS-updated global strings (cur_date, hhmm_str, am_pm_str)
                String timestamp;
                if (gpsMutex != NULL && xSemaphoreTake(gpsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    timestamp = cur_date + "  " + hhmm_str + am_pm_str;
                    xSemaphoreGive(gpsMutex);
                } else {
                    timestamp = "GPS data unavailable";
                }

                hotPacketBuffer_np_rcv_time[backBuffer] = timestamp;
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

// SAFEGUARD #3: Helper function to validate and sanitize field length
// Returns sanitized string, or empty string if validation fails
String validateField(const char* field, int maxLength, const char* fieldName) {
    if (!field) {
        Serial.print("Field ");
        Serial.print(fieldName);
        Serial.println(" is NULL");
        return "";
    }

    String fieldStr = String(field);
    int fieldLen = fieldStr.length();

    if (fieldLen == 0) {
        Serial.print("Field ");
        Serial.print(fieldName);
        Serial.println(" is empty");
        return "";
    }

    if (fieldLen > maxLength) {
        Serial.print("Field ");
        Serial.print(fieldName);
        Serial.print(" too long (");
        Serial.print(fieldLen);
        Serial.print(" > ");
        Serial.print(maxLength);
        Serial.println("), truncating");
        fieldStr = fieldStr.substring(0, maxLength);
    }

    return fieldStr;
}

// SAFEGUARD #3: Helper function to validate and sanitize temperature field
String validateTemperature(const char* tempField, const char* fieldName) {
    String tempStr = validateField(tempField, 10, fieldName);  // Allow up to 10 chars for parsing
    if (tempStr.length() == 0) return "";

    // Convert to integer and clamp to valid range
    int tempInt = tempStr.toInt();
    if (tempInt < -99) tempInt = -99;
    if (tempInt > 999) tempInt = 999;

    return String(tempInt);
}

int parseWeatherData(char* input, const String& timestamp) {
    if (!input) {
        Serial.println("Weather packet: NULL input");
        return 0;
    }

    int ptr, len;
    len = strlen(input);

    // Validate minimum packet size
    if (len < HOT_PKT_HEADER_OFFSET + 1) {
        Serial.println("Weather packet too short");
        return 0;
    }

    // SAFEGUARD #2: Count delimiters to validate packet structure
    // Expected format: |#01#temp#hr,glyph,temp,precip#hr,glyph,temp,precip#hr,glyph,temp,precip#hr,glyph,temp,precip#
    // Count: 7x '#' and 12x ',' = 19 delimiters total
    // Example: |#01#100.0#10am,4,77,1.2#11am,4,78,0.0#12pm,7,79,0.03#1pm,7,80,0.01#
    //           1   2     3          4            5             6              7
    int hashCount = 0;
    int commaCount = 0;
    for (int i = 0; i < len; i++) {
        if (input[i] == '#') hashCount++;
        if (input[i] == ',') commaCount++;
    }

    if (hashCount != 7 || commaCount != 12) {
        Serial.print("Weather packet malformed - expected 7 '#' and 12 ',', got ");
        Serial.print(hashCount);
        Serial.print(" '#' and ");
        Serial.print(commaCount);
        Serial.println(" ','");
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

    // SAFEGUARD #1: Clear back buffer before parsing to prevent stale data contamination
    hotPacketBuffer_wx_rcv_time[backBuffer] = "";
    hotPacketBuffer_cur_temp[backBuffer] = "";
    hotPacketBuffer_fcast_hr1[backBuffer] = "";
    hotPacketBuffer_fcast_glyph1[backBuffer] = "";
    hotPacketBuffer_fcast_temp1[backBuffer] = "";
    hotPacketBuffer_fcast_precip1[backBuffer] = "";
    hotPacketBuffer_fcast_hr2[backBuffer] = "";
    hotPacketBuffer_fcast_glyph2[backBuffer] = "";
    hotPacketBuffer_fcast_temp2[backBuffer] = "";
    hotPacketBuffer_fcast_precip2[backBuffer] = "";
    hotPacketBuffer_fcast_hr3[backBuffer] = "";
    hotPacketBuffer_fcast_glyph3[backBuffer] = "";
    hotPacketBuffer_fcast_temp3[backBuffer] = "";
    hotPacketBuffer_fcast_precip3[backBuffer] = "";
    hotPacketBuffer_fcast_hr4[backBuffer] = "";
    hotPacketBuffer_fcast_glyph4[backBuffer] = "";
    hotPacketBuffer_fcast_temp4[backBuffer] = "";
    hotPacketBuffer_fcast_precip4[backBuffer] = "";

    // Write timestamp to back buffer
    hotPacketBuffer_wx_rcv_time[backBuffer] = timestamp;

    // SAFEGUARD #4: Transaction-style parsing - use tmp variables, only commit if all fields valid
    String tmp_cur_temp, tmp_hr1, tmp_glyph1, tmp_temp1, tmp_precip1;
    String tmp_hr2, tmp_glyph2, tmp_temp2, tmp_precip2;
    String tmp_hr3, tmp_glyph3, tmp_temp3, tmp_precip3;
    String tmp_hr4, tmp_glyph4, tmp_temp4, tmp_precip4;

    // Parse and validate cur_temp
    if (ptr >= len) {
        Serial.println("Parse error: unexpected end at cur_temp");
        return 0;
    }
    tmp_cur_temp = validateField(&input[ptr], 10, "cur_temp");
    if (tmp_cur_temp.length() == 0) return 0;
    ptr = ptr + strlen(&input[ptr]) + 1;

    // Parse forecast hour 1
    if (ptr >= len) {
        Serial.println("Parse error: unexpected end at fcast_hr1");
        return 0;
    }
    tmp_hr1 = validateField(&input[ptr], 6, "fcast_hr1");  // Max 6: "12pm" + null
    if (tmp_hr1.length() == 0) return 0;
    ptr = ptr + strlen(&input[ptr]) + 1;

    if (ptr >= len) {
        Serial.println("Parse error: unexpected end at fcast_glyph1");
        return 0;
    }
    tmp_glyph1 = validateField(&input[ptr], 2, "fcast_glyph1");  // Max 2: single digit glyph code
    if (tmp_glyph1.length() == 0) return 0;
    ptr = ptr + strlen(&input[ptr]) + 1;

    if (ptr >= len) {
        Serial.println("Parse error: unexpected end at fcast_temp1");
        return 0;
    }
    tmp_temp1 = validateTemperature(&input[ptr], "fcast_temp1");
    if (tmp_temp1.length() == 0) return 0;
    ptr = ptr + strlen(&input[ptr]) + 1;

    if (ptr >= len) {
        Serial.println("Parse error: unexpected end at fcast_precip1");
        return 0;
    }
    tmp_precip1 = validateField(&input[ptr], 6, "fcast_precip1");  // Max 6: "99.99" + null
    if (tmp_precip1.length() == 0) return 0;
    if (tmp_precip1 == "0.0") tmp_precip1 = "";  // Clear zero precipitation
    ptr = ptr + strlen(&input[ptr]) + 1;

    // Parse forecast hour 2
    if (ptr >= len) {
        Serial.println("Parse error: unexpected end at fcast_hr2");
        return 0;
    }
    tmp_hr2 = validateField(&input[ptr], 6, "fcast_hr2");
    if (tmp_hr2.length() == 0) return 0;
    ptr = ptr + strlen(&input[ptr]) + 1;

    if (ptr >= len) {
        Serial.println("Parse error: unexpected end at fcast_glyph2");
        return 0;
    }
    tmp_glyph2 = validateField(&input[ptr], 2, "fcast_glyph2");
    if (tmp_glyph2.length() == 0) return 0;
    ptr = ptr + strlen(&input[ptr]) + 1;

    if (ptr >= len) {
        Serial.println("Parse error: unexpected end at fcast_temp2");
        return 0;
    }
    tmp_temp2 = validateTemperature(&input[ptr], "fcast_temp2");
    if (tmp_temp2.length() == 0) return 0;
    ptr = ptr + strlen(&input[ptr]) + 1;

    if (ptr >= len) {
        Serial.println("Parse error: unexpected end at fcast_precip2");
        return 0;
    }
    tmp_precip2 = validateField(&input[ptr], 6, "fcast_precip2");
    if (tmp_precip2.length() == 0) return 0;
    if (tmp_precip2 == "0.0") tmp_precip2 = "";
    ptr = ptr + strlen(&input[ptr]) + 1;

    // Parse forecast hour 3
    if (ptr >= len) {
        Serial.println("Parse error: unexpected end at fcast_hr3");
        return 0;
    }
    tmp_hr3 = validateField(&input[ptr], 6, "fcast_hr3");
    if (tmp_hr3.length() == 0) return 0;
    ptr = ptr + strlen(&input[ptr]) + 1;

    if (ptr >= len) {
        Serial.println("Parse error: unexpected end at fcast_glyph3");
        return 0;
    }
    tmp_glyph3 = validateField(&input[ptr], 2, "fcast_glyph3");
    if (tmp_glyph3.length() == 0) return 0;
    ptr = ptr + strlen(&input[ptr]) + 1;

    if (ptr >= len) {
        Serial.println("Parse error: unexpected end at fcast_temp3");
        return 0;
    }
    tmp_temp3 = validateTemperature(&input[ptr], "fcast_temp3");
    if (tmp_temp3.length() == 0) return 0;
    ptr = ptr + strlen(&input[ptr]) + 1;

    if (ptr >= len) {
        Serial.println("Parse error: unexpected end at fcast_precip3");
        return 0;
    }
    tmp_precip3 = validateField(&input[ptr], 6, "fcast_precip3");
    if (tmp_precip3.length() == 0) return 0;
    if (tmp_precip3 == "0.0") tmp_precip3 = "";
    ptr = ptr + strlen(&input[ptr]) + 1;

    // Parse forecast hour 4
    if (ptr >= len) {
        Serial.println("Parse error: unexpected end at fcast_hr4");
        return 0;
    }
    tmp_hr4 = validateField(&input[ptr], 6, "fcast_hr4");
    if (tmp_hr4.length() == 0) return 0;
    ptr = ptr + strlen(&input[ptr]) + 1;

    if (ptr >= len) {
        Serial.println("Parse error: unexpected end at fcast_glyph4");
        return 0;
    }
    tmp_glyph4 = validateField(&input[ptr], 2, "fcast_glyph4");
    if (tmp_glyph4.length() == 0) return 0;
    ptr = ptr + strlen(&input[ptr]) + 1;

    if (ptr >= len) {
        Serial.println("Parse error: unexpected end at fcast_temp4");
        return 0;
    }
    tmp_temp4 = validateTemperature(&input[ptr], "fcast_temp4");
    if (tmp_temp4.length() == 0) return 0;
    ptr = ptr + strlen(&input[ptr]) + 1;

    if (ptr >= len) {
        Serial.println("Parse error: unexpected end at fcast_precip4");
        return 0;
    }
    tmp_precip4 = validateField(&input[ptr], 6, "fcast_precip4");
    if (tmp_precip4.length() == 0) return 0;
    if (tmp_precip4 == "0.0") tmp_precip4 = "";
    ptr = ptr + strlen(&input[ptr]) + 1;

    // SAFEGUARD #4: All fields parsed successfully - commit to back buffer atomically
    hotPacketBuffer_cur_temp[backBuffer] = tmp_cur_temp;
    hotPacketBuffer_fcast_hr1[backBuffer] = tmp_hr1;
    hotPacketBuffer_fcast_glyph1[backBuffer] = tmp_glyph1;
    hotPacketBuffer_fcast_temp1[backBuffer] = tmp_temp1;
    hotPacketBuffer_fcast_precip1[backBuffer] = tmp_precip1;
    hotPacketBuffer_fcast_hr2[backBuffer] = tmp_hr2;
    hotPacketBuffer_fcast_glyph2[backBuffer] = tmp_glyph2;
    hotPacketBuffer_fcast_temp2[backBuffer] = tmp_temp2;
    hotPacketBuffer_fcast_precip2[backBuffer] = tmp_precip2;
    hotPacketBuffer_fcast_hr3[backBuffer] = tmp_hr3;
    hotPacketBuffer_fcast_glyph3[backBuffer] = tmp_glyph3;
    hotPacketBuffer_fcast_temp3[backBuffer] = tmp_temp3;
    hotPacketBuffer_fcast_precip3[backBuffer] = tmp_precip3;
    hotPacketBuffer_fcast_hr4[backBuffer] = tmp_hr4;
    hotPacketBuffer_fcast_glyph4[backBuffer] = tmp_glyph4;
    hotPacketBuffer_fcast_temp4[backBuffer] = tmp_temp4;
    hotPacketBuffer_fcast_precip4[backBuffer] = tmp_precip4;

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
        fcast_temp1 = hotPacketBuffer_fcast_temp1[backBuffer];
        fcast_precip1 = hotPacketBuffer_fcast_precip1[backBuffer];
        fcast_hr2 = hotPacketBuffer_fcast_hr2[backBuffer];
        fcast_glyph2 = hotPacketBuffer_fcast_glyph2[backBuffer];
        fcast_temp2 = hotPacketBuffer_fcast_temp2[backBuffer];
        fcast_precip2 = hotPacketBuffer_fcast_precip2[backBuffer];
        fcast_hr3 = hotPacketBuffer_fcast_hr3[backBuffer];
        fcast_glyph3 = hotPacketBuffer_fcast_glyph3[backBuffer];
        fcast_temp3 = hotPacketBuffer_fcast_temp3[backBuffer];
        fcast_precip3 = hotPacketBuffer_fcast_precip3[backBuffer];
        fcast_hr4 = hotPacketBuffer_fcast_hr4[backBuffer];
        fcast_glyph4 = hotPacketBuffer_fcast_glyph4[backBuffer];
        fcast_temp4 = hotPacketBuffer_fcast_temp4[backBuffer];
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