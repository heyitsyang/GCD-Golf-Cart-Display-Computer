#include "hot_packet_parser.h"
#include "config.h"
#include "globals.h"
#include "types.h"
#include "utils/time_utils.h"
#include "storage/preferences_manager.h"
#include "mailbox.h"

bool isHotPacket(const char* text) {
    return (text != NULL && text[0] == '|');
}

// Pointer to '#'-delimited field `idx` (0-based) of s; length into *outLen.
// NULL if absent or empty. Tolerates a missing trailing '#'. Never modifies input.
const char* hotField(const char* s, uint8_t idx, uint8_t* outLen) {
    if (!s || !outLen) return NULL;
    *outLen = 0;
    for (uint8_t i = 0; i < idx; i++) {
        const char* sep = strchr(s, '#');
        if (!sep) return NULL;       // ran out of fields
        s = sep + 1;
    }
    const char* end = strchr(s, '#');
    size_t len = end ? (size_t)(end - s) : strlen(s);
    if (len == 0 || len > 255) return NULL;
    *outLen = (uint8_t)len;
    return s;
}

// Exactly 8 hex digits -> uint32. Case-insensitive. false otherwise.
bool hotParseHex8(const char* p, uint8_t len, uint32_t* out) {
    if (!p || !out || len != 8) return false;
    uint32_t v = 0;
    for (uint8_t i = 0; i < 8; i++) {
        char c = p[i];
        uint32_t d;
        if      (c >= '0' && c <= '9') d = (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = (uint32_t)(c - 'A' + 10);
        else return false;
        v = (v << 4) | d;
    }
    *out = v;
    return true;
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
#if DEBUG_GCM_MESSAGES
    Serial.print("GCM RX HoT pkt: ");
#endif

    // Set flag for any HOT packet received (for UI updates)
    new_rx_data_flag = true;

    int HotPktType = parseHotPacketType(text);

    if (HotPktType == -1) {
        Serial.print("Malformed HoT packet type, raw: ");
        // Print first 20 chars of packet for diagnosis
        char preview[21];
        strncpy(preview, text, 20);
        preview[20] = '\0';
        Serial.println(preview);
        return;
    }
    
    switch (HotPktType) {
        case HOT_PACKET_WEATHER: {
#if DEBUG_GCM_MESSAGES
            Serial.println("WX packet received");
#endif

            // Save raw packet before parseWeatherData() destroys the buffer in-place
            String rawWxPacket = String(text);

            // Protect access to GPS-updated global strings (cur_date, hhmm_str, am_pm_str)
            // Show actual timestamp if GPS is working, otherwise indicate timestamp unavailable
            String timestamp;
            int gpsYear = 0, gpsMonth = 0, gpsDay = 0;
            if (gpsMutex != NULL && xSemaphoreTake(gpsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (cur_date != "NO GPS" && hhmm_str.length() > 0) {
                    timestamp = cur_date + "  " + hhmm_str + am_pm_str;
                } else {
                    timestamp = "TIMESTAMP UNAVAILABLE";
                }
                gpsYear = localYear;
                gpsMonth = localMonth;
                gpsDay = localDay;
                xSemaphoreGive(gpsMutex);
            } else {
                timestamp = "TIMESTAMP UNAVAILABLE";
            }

            // Parse weather data (updates buffers and swaps)
            // Pass the timestamp to be written to the same back buffer
            if (parseWeatherData((char*)text, timestamp)) {
                // Live packet received - clear stored flag
                wx_data_is_stored = false;
                // Update legacy variable for compatibility
                wx_rcv_time = hotPacketBuffer_wx_rcv_time[hotPacketActiveBufferWx];

                // Persist to EEPROM if GPS date is valid
                if (gpsYear != 0) {
                    int todayYYYYMMDD = gpsYear * 10000 + gpsMonth * 100 + gpsDay;
                    if (todayYYYYMMDD != wx_stored_date || rawWxPacket != wx_stored_data) {
                        if (xSemaphoreTake(eepromMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                            prefs.putString("wx_data", rawWxPacket.c_str());
                            prefs.putString("wx_time", timestamp.c_str());
                            xSemaphoreGive(eepromMutex);
                        }
                        queuePreferenceWrite("wx_date", todayYYYYMMDD);
                        wx_stored_date = todayYYYYMMDD;
                        wx_stored_data = rawWxPacket;
                        wx_stored_timestamp = timestamp;
                    }
                }
            }
            break;
        }

        case HOT_PACKET_VENUE_EVENT: {
            // Example: |#02#Sawgrass,Trivia Tuesday#Spanish Springs,5.0.1.#Lake Sumter,Zee-R Band#Brownwood,Hacksaw Hamlin#Sawgrass,Steve Hogie Band#Eastport,Boozy Bingo-Eastport#
#if DEBUG_GCM_MESSAGES
            Serial.println("Venue/Event packet received");
#endif
            // Validate packet has enough data
            if (strlen(text) > HOT_PKT_HEADER_OFFSET) {
                // Write to back buffer (whichever is NOT active)
                int backBuffer = 1 - hotPacketActiveBufferNp;

                // Protect access to GPS-updated global strings (cur_date, hhmm_str, am_pm_str)
                // Show actual timestamp if GPS is working, otherwise indicate timestamp unavailable
                String timestamp;
                int gpsYear = 0, gpsMonth = 0, gpsDay = 0;
                if (gpsMutex != NULL && xSemaphoreTake(gpsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    if (cur_date != "NO GPS" && hhmm_str.length() > 0) {
                        timestamp = cur_date + "  " + hhmm_str + am_pm_str;
                    } else {
                        timestamp = "TIMESTAMP UNAVAILABLE";
                    }
                    gpsYear = localYear;
                    gpsMonth = localMonth;
                    gpsDay = localDay;
                    xSemaphoreGive(gpsMutex);
                } else {
                    timestamp = "TIMESTAMP UNAVAILABLE";
                }

                strncpy(hotPacketBuffer_np_rcv_time[backBuffer], timestamp.c_str(), HP_RCV_TIME_SIZE - 1);
                hotPacketBuffer_np_rcv_time[backBuffer][HP_RCV_TIME_SIZE - 1] = '\0';
                strncpy(hotPacketBuffer_live_venue_event_data[backBuffer], &text[HOT_PKT_HEADER_OFFSET], HP_VENUE_DATA_SIZE - 1);
                hotPacketBuffer_live_venue_event_data[backBuffer][HP_VENUE_DATA_SIZE - 1] = '\0';

                // Atomically swap buffers (very fast, no blocking for GUI)
                // Mutex only protects the pointer swap, not data reads
                bool haveMutex = (hotPacketMutex != NULL && xSemaphoreTake(hotPacketMutex, pdMS_TO_TICKS(10)) == pdTRUE);
                if (haveMutex || hotPacketMutex == NULL) {
                    hotPacketActiveBufferNp = backBuffer;
                    if (haveMutex) xSemaphoreGive(hotPacketMutex);

                    // Live packet received - clear stored flag
                    np_data_is_stored = false;

                    // Persist to EEPROM if GPS date is valid
                    if (gpsYear != 0) {
                        int todayYYYYMMDD = gpsYear * 10000 + gpsMonth * 100 + gpsDay;
                        String newData = String(text);  // Store full received form
                        if (todayYYYYMMDD != np_stored_date || newData != np_stored_data) {
                            if (xSemaphoreTake(eepromMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                                prefs.putString("np_data", newData.c_str());
                                prefs.putString("np_time", timestamp.c_str());
                                xSemaphoreGive(eepromMutex);
                            }
                            queuePreferenceWrite("np_date", todayYYYYMMDD);
                            np_stored_date = todayYYYYMMDD;
                            np_stored_data = newData;
                            np_stored_timestamp = timestamp;
                        }
                    }
                } else {
                    Serial.println("Buffer swap timeout (venue data)");
                }
            } else {
                Serial.println("Venue/Event packet too short");
            }
            break;
        }

        case HOT_PACKET_MAILBOX: {
            // |#03#NORM#a1b2c3d4#5#87#142#PRESENT      (mode also: DEV | PAIR)
            // Unlike the weather branch, this one is const-only — it must not
            // tokenize the buffer in place.
            const char* body = &text[HOT_PKT_HEADER_OFFSET];   // "NORM#a1b2c3d4#..."
            uint8_t mdLen = 0, idLen = 0, sqLen = 0, stLen = 0;
            const char* mdp = hotField(body, 0, &mdLen);       // mode
            const char* idp = hotField(body, 1, &idLen);       // mbx_id
            uint32_t mbxId = 0;
            if (!idp || !hotParseHex8(idp, idLen, &mbxId)) {
                Serial.println("MBX malformed");
                break;
            }

            // seq drives missed-frame counting. Absent or unparseable is not an
            // error — the frame is still usable, gap counting is just skipped.
            const char* sqp = hotField(body, 2, &sqLen);
            uint8_t  seq      = 0;
            bool     seqValid = false;
            if (sqp && sqLen <= 3) {
                uint32_t v = 0;
                seqValid = true;
                for (uint8_t i = 0; i < sqLen; i++) {
                    if (sqp[i] < '0' || sqp[i] > '9') { seqValid = false; break; }
                    v = v * 10 + (uint32_t)(sqp[i] - '0');
                }
                if (seqValid && v <= 255) seq = (uint8_t)v;
                else                      seqValid = false;
            }

            bool isPairOffer = (mdp && mdLen == 4 && memcmp(mdp, "PAIR", 4) == 0);

            // A PAIR frame carries only mode + mbx_id as trustworthy fields, so
            // its state is not read; it still counts as contact for health.
            if (isPairOffer) {
                mailboxOnFrame(mbxId, false, true, seq, seqValid);
                break;
            }

            const char* stp = hotField(body, 5, &stLen);       // state
            bool present;
            if      (stp && stLen == 7 && memcmp(stp, "PRESENT", 7) == 0) present = true;
            else if (stp && stLen == 6 && memcmp(stp, "ABSENT",  6) == 0) present = false;
            else { Serial.println("MBX bad state"); break; }

            // Unknown modes (e.g. DEV) fall through as NORM — forward-compatible.
            mailboxOnFrame(mbxId, present, false, seq, seqValid);
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
        Serial.print(" ',', raw: ");
        char preview[41];
        strncpy(preview, input, 40);
        preview[40] = '\0';
        Serial.println(preview);
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
    int backBuffer = 1 - hotPacketActiveBufferWx;

    // SAFEGUARD #1: Clear back buffer before parsing to prevent stale data contamination
    hotPacketBuffer_wx_rcv_time[backBuffer][0] = '\0';
    hotPacketBuffer_cur_temp[backBuffer][0] = '\0';
    hotPacketBuffer_fcast_hr1[backBuffer][0] = '\0';
    hotPacketBuffer_fcast_glyph1[backBuffer][0] = '\0';
    hotPacketBuffer_fcast_temp1[backBuffer][0] = '\0';
    hotPacketBuffer_fcast_precip1[backBuffer][0] = '\0';
    hotPacketBuffer_fcast_hr2[backBuffer][0] = '\0';
    hotPacketBuffer_fcast_glyph2[backBuffer][0] = '\0';
    hotPacketBuffer_fcast_temp2[backBuffer][0] = '\0';
    hotPacketBuffer_fcast_precip2[backBuffer][0] = '\0';
    hotPacketBuffer_fcast_hr3[backBuffer][0] = '\0';
    hotPacketBuffer_fcast_glyph3[backBuffer][0] = '\0';
    hotPacketBuffer_fcast_temp3[backBuffer][0] = '\0';
    hotPacketBuffer_fcast_precip3[backBuffer][0] = '\0';
    hotPacketBuffer_fcast_hr4[backBuffer][0] = '\0';
    hotPacketBuffer_fcast_glyph4[backBuffer][0] = '\0';
    hotPacketBuffer_fcast_temp4[backBuffer][0] = '\0';
    hotPacketBuffer_fcast_precip4[backBuffer][0] = '\0';

    // Write timestamp to back buffer
    strncpy(hotPacketBuffer_wx_rcv_time[backBuffer], timestamp.c_str(), HP_RCV_TIME_SIZE - 1);
    hotPacketBuffer_wx_rcv_time[backBuffer][HP_RCV_TIME_SIZE - 1] = '\0';

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
    strncpy(hotPacketBuffer_cur_temp[backBuffer],     tmp_cur_temp.c_str(), HP_CUR_TEMP_SIZE - 1);   hotPacketBuffer_cur_temp[backBuffer][HP_CUR_TEMP_SIZE - 1] = '\0';
    strncpy(hotPacketBuffer_fcast_hr1[backBuffer],    tmp_hr1.c_str(),      HP_FCAST_HR_SIZE - 1);    hotPacketBuffer_fcast_hr1[backBuffer][HP_FCAST_HR_SIZE - 1] = '\0';
    strncpy(hotPacketBuffer_fcast_glyph1[backBuffer], tmp_glyph1.c_str(),   HP_FCAST_GLYPH_SIZE - 1); hotPacketBuffer_fcast_glyph1[backBuffer][HP_FCAST_GLYPH_SIZE - 1] = '\0';
    strncpy(hotPacketBuffer_fcast_temp1[backBuffer],  tmp_temp1.c_str(),    HP_FCAST_TEMP_SIZE - 1);  hotPacketBuffer_fcast_temp1[backBuffer][HP_FCAST_TEMP_SIZE - 1] = '\0';
    strncpy(hotPacketBuffer_fcast_precip1[backBuffer],tmp_precip1.c_str(),  HP_FCAST_PRECIP_SIZE - 1);hotPacketBuffer_fcast_precip1[backBuffer][HP_FCAST_PRECIP_SIZE - 1] = '\0';
    strncpy(hotPacketBuffer_fcast_hr2[backBuffer],    tmp_hr2.c_str(),      HP_FCAST_HR_SIZE - 1);    hotPacketBuffer_fcast_hr2[backBuffer][HP_FCAST_HR_SIZE - 1] = '\0';
    strncpy(hotPacketBuffer_fcast_glyph2[backBuffer], tmp_glyph2.c_str(),   HP_FCAST_GLYPH_SIZE - 1); hotPacketBuffer_fcast_glyph2[backBuffer][HP_FCAST_GLYPH_SIZE - 1] = '\0';
    strncpy(hotPacketBuffer_fcast_temp2[backBuffer],  tmp_temp2.c_str(),    HP_FCAST_TEMP_SIZE - 1);  hotPacketBuffer_fcast_temp2[backBuffer][HP_FCAST_TEMP_SIZE - 1] = '\0';
    strncpy(hotPacketBuffer_fcast_precip2[backBuffer],tmp_precip2.c_str(),  HP_FCAST_PRECIP_SIZE - 1);hotPacketBuffer_fcast_precip2[backBuffer][HP_FCAST_PRECIP_SIZE - 1] = '\0';
    strncpy(hotPacketBuffer_fcast_hr3[backBuffer],    tmp_hr3.c_str(),      HP_FCAST_HR_SIZE - 1);    hotPacketBuffer_fcast_hr3[backBuffer][HP_FCAST_HR_SIZE - 1] = '\0';
    strncpy(hotPacketBuffer_fcast_glyph3[backBuffer], tmp_glyph3.c_str(),   HP_FCAST_GLYPH_SIZE - 1); hotPacketBuffer_fcast_glyph3[backBuffer][HP_FCAST_GLYPH_SIZE - 1] = '\0';
    strncpy(hotPacketBuffer_fcast_temp3[backBuffer],  tmp_temp3.c_str(),    HP_FCAST_TEMP_SIZE - 1);  hotPacketBuffer_fcast_temp3[backBuffer][HP_FCAST_TEMP_SIZE - 1] = '\0';
    strncpy(hotPacketBuffer_fcast_precip3[backBuffer],tmp_precip3.c_str(),  HP_FCAST_PRECIP_SIZE - 1);hotPacketBuffer_fcast_precip3[backBuffer][HP_FCAST_PRECIP_SIZE - 1] = '\0';
    strncpy(hotPacketBuffer_fcast_hr4[backBuffer],    tmp_hr4.c_str(),      HP_FCAST_HR_SIZE - 1);    hotPacketBuffer_fcast_hr4[backBuffer][HP_FCAST_HR_SIZE - 1] = '\0';
    strncpy(hotPacketBuffer_fcast_glyph4[backBuffer], tmp_glyph4.c_str(),   HP_FCAST_GLYPH_SIZE - 1); hotPacketBuffer_fcast_glyph4[backBuffer][HP_FCAST_GLYPH_SIZE - 1] = '\0';
    strncpy(hotPacketBuffer_fcast_temp4[backBuffer],  tmp_temp4.c_str(),    HP_FCAST_TEMP_SIZE - 1);  hotPacketBuffer_fcast_temp4[backBuffer][HP_FCAST_TEMP_SIZE - 1] = '\0';
    strncpy(hotPacketBuffer_fcast_precip4[backBuffer],tmp_precip4.c_str(),  HP_FCAST_PRECIP_SIZE - 1);hotPacketBuffer_fcast_precip4[backBuffer][HP_FCAST_PRECIP_SIZE - 1] = '\0';

    // Atomically swap buffers (very fast, no blocking for GUI)
    // Mutex only protects the pointer swap, not data reads (10ms timeout is very short)
    bool haveMutex = (hotPacketMutex != NULL && xSemaphoreTake(hotPacketMutex, pdMS_TO_TICKS(10)) == pdTRUE);
    if (haveMutex || hotPacketMutex == NULL) {
        hotPacketActiveBufferWx = backBuffer;
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

        return 1;
    } else {
        Serial.println("Buffer swap timeout (weather data)");
        return 0;
    }
}

void parseVenueEventData(const char* input) {
    // This function can be expanded to parse venue/event data into structured format
    // Raw data is stored in hotPacketBuffer_live_venue_event_data double-buffer
}