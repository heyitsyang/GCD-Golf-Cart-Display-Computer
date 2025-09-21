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
        case HOT_PACKET_WEATHER:
            Serial.println("WX packet received");
            wx_rcv_time = cur_date + "  " + hhmm_str + am_pm_str;
            parseWeatherData((char*)text);
            break;
            
        case HOT_PACKET_VENUE_EVENT:
            Serial.println("Venue/Event packet received");
            np_rcv_time = cur_date + "  " + hhmm_str + am_pm_str;
            live_venue_event_data = String(&text[HOT_PKT_HEADER_OFFSET]);
            Serial.print("Stored venue/event data: ");
            Serial.println(live_venue_event_data);
            break;
            
        default:
            Serial.print("Unrecognized HotPktType: ");
            Serial.println(HotPktType);
            break;
    }
}

int parseWeatherData(char* input) {
    int ptr, len;
    len = strlen(input);
    
    if (!input) return 0;
    
    // Replace delimiters with null terminators
    for (ptr = 0; ptr < len; ptr++) {
        if ((input[ptr] == '#') || (input[ptr] == ',')) {
            input[ptr] = '\0';
        }
    }
    
    ptr = HOT_PKT_HEADER_OFFSET;
    cur_temp = String(&input[ptr]);
    ptr = ptr + strlen(&input[ptr]) + 1;
    
    fcast_hr1 = String(&input[ptr]);
    ptr = ptr + fcast_hr1.length() + 1;
    fcast_glyph1 = String(&input[ptr]);
    ptr = ptr + fcast_glyph1.length() + 1;
    fcast_precip1 = String(&input[ptr]);
    int precip1_len = fcast_precip1.length();
    if (fcast_precip1 == "0.0") fcast_precip1 = "";
    ptr = ptr + precip1_len + 1;

    fcast_hr2 = String(&input[ptr]);
    ptr = ptr + fcast_hr2.length() + 1;
    fcast_glyph2 = String(&input[ptr]);
    ptr = ptr + fcast_glyph2.length() + 1;
    fcast_precip2 = String(&input[ptr]);
    int precip2_len = fcast_precip2.length();
    if (fcast_precip2 == "0.0") fcast_precip2 = "";
    ptr = ptr + precip2_len + 1;

    fcast_hr3 = String(&input[ptr]);
    ptr = ptr + fcast_hr3.length() + 1;
    fcast_glyph3 = String(&input[ptr]);
    ptr = ptr + fcast_glyph3.length() + 1;
    fcast_precip3 = String(&input[ptr]);
    int precip3_len = fcast_precip3.length();
    if (fcast_precip3 == "0.0") fcast_precip3 = "";
    ptr = ptr + precip3_len + 1;

    fcast_hr4 = String(&input[ptr]);
    ptr = ptr + fcast_hr4.length() + 1;
    fcast_glyph4 = String(&input[ptr]);
    ptr = ptr + fcast_glyph4.length() + 1;
    fcast_precip4 = String(&input[ptr]);
    int precip4_len = fcast_precip4.length();
    if (fcast_precip4 == "0.0") fcast_precip4 = "";
    ptr = ptr + precip4_len + 1;
    
    return 1;
}

void parseVenueEventData(const char* input) {
    // This function can be expanded to parse venue/event data into structured format
    // For now, the raw data is stored in live_venue_event_data
}