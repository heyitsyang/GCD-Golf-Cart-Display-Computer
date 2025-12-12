#include "get_set_vars.h"
#include <Arduino.h>
#include "storage/preferences_manager.h"
#include "hardware/display.h"
#include "communication/espnow_handler.h"
#include "globals.h"

// String variable definitions
String cur_date;
String heading;
String hhmmss_str;
String hhmm_str;
String am_pm_str;
String sats_hdop;
String version;
String cyd_mac_addr;
String espnow_gci_mac_addr;
String wx_rcv_time;
String cur_temp;
String fcast_hr1;
String fcast_glyph1;
String fcast_temp1;
String fcast_precip1;
String fcast_hr2;
String fcast_glyph2;
String fcast_temp2;
String fcast_precip2;
String fcast_hr3;
String fcast_glyph3;
String fcast_temp3;
String fcast_precip3;
String fcast_hr4;
String fcast_glyph4;
String fcast_temp4;
String fcast_precip4;
String np_rcv_time;
String espnow_status;
String espnow_last_received;
String gcm_node_id;
String text_message;

// Numeric variable definitions
int32_t avg_speed = 0;
int32_t day_backlight = 10;
int32_t night_backlight = 5;
bool manual_reboot = false;
bool new_rx_data_flag = false;
bool mesh_serial_enabled = true;
bool espnow_connected = false;
bool espnow_pair_gci = false;
int32_t screen_inactivity_countdown = 0;
bool flip_screen = false;
int32_t speaker_volume = 10;
String odometer = "0.0";
String trip_odometer = "0.0";
int32_t hrs_since_svc = 0;
int32_t svc_interval_hrs = 100;
float accumDistance = 0.0;
float tripDistance = 0.0;
bool reset_preferences = false;
float temperature_adj = 0;
bool reboot_meshtastic = false;

// Static buffers for C string returns
static char temp_buffer[256];

// Function implementations with C linkage
extern "C" {

const char* get_var_cur_date() {
    return cur_date.c_str();
}

void set_var_cur_date(const char* value) {
    cur_date = String(value);
}

const char* get_var_heading() {
    return heading.c_str();
}

void set_var_heading(const char* value) {
    heading = String(value);
}

const char* get_var_hhmmss_str() {
    return hhmmss_str.c_str();
}

void set_var_hhmmss_str(const char* value) {
    hhmmss_str = String(value);
}

const char* get_var_hhmm_str() {
    return hhmm_str.c_str();
}

void set_var_hhmm_str(const char* value) {
    hhmm_str = String(value);
}

const char* get_var_am_pm_str() {
    return am_pm_str.c_str();
}

void set_var_am_pm_str(const char* value) {
    am_pm_str = String(value);
}

const char* get_var_sats_hdop() {
    return sats_hdop.c_str();
}

void set_var_sats_hdop(const char* value) {
    sats_hdop = String(value);
}

int32_t get_var_avg_speed() {
    return avg_speed;
}

void set_var_avg_speed(int32_t value) {
    avg_speed = value;
}

const char* get_var_version() {
    return version.c_str();
}

void set_var_version(const char* value) {
    version = String(value);
}

int32_t get_var_cyd_day_backlight() {
    return day_backlight;
}

void set_var_cyd_day_backlight(int32_t value) {
    day_backlight = value;
}

int32_t get_var_cyd_night_backlight() {
    return night_backlight;
}

void set_var_cyd_night_backlight(int32_t value) {
    night_backlight = value;
}

const char* get_var_cyd_mac_addr() {
    return cyd_mac_addr.c_str();
}

void set_var_cyd_mac_addr(const char* value) {
    cyd_mac_addr = String(value);
}

bool get_var_manual_reboot() {
    return manual_reboot;
}

void set_var_manual_reboot(bool value) {
    manual_reboot = value;
}

bool get_var_new_rx_data_flag() {
    return new_rx_data_flag;
}

void set_var_new_rx_data_flag(bool value) {
    new_rx_data_flag = value;
}

const char* get_var_espnow_gci_mac_addr() {
    return espnow_gci_mac_addr.c_str();
}

void set_var_espnow_gci_mac_addr(const char* value) {
    String new_mac = String(value);

    if (espnow_gci_mac_addr != new_mac) {
        Serial.print("ESP-NOW GCI MAC address changed from ");
        Serial.print(espnow_gci_mac_addr);
        Serial.print(" to ");
        Serial.println(new_mac);

        espnow_gci_mac_addr = new_mac;

        // Restart ESP-NOW if it's currently enabled and initialized
        if (espnow_enabled && espNow.isInitialized()) {
            Serial.println("Restarting ESP-NOW with new peer MAC address...");

            if (espNow.restart()) {
                // Add the new peer if it's valid
                if (espnow_gci_mac_addr != "NONE" && espnow_gci_mac_addr.length() == 17) {
                    if (espNow.addPeerFromString(espnow_gci_mac_addr, "New Peer")) {
                        Serial.println("ESP-NOW: New peer added successfully");
                    } else {
                        Serial.println("ESP-NOW: Failed to add new peer");
                    }
                }
            } else {
                Serial.println("ESP-NOW: Restart failed");
            }
        }

        // Queue the preference write to save to EEPROM
        queuePreferenceWrite("espnow_gci_mac_addr", espnow_gci_mac_addr);
    }
}

bool get_var_mesh_serial_enabled() {
    return mesh_serial_enabled;
}

void set_var_mesh_serial_enabled(bool value) {
    mesh_serial_enabled = value;
}

bool get_var_espnow_connected() {
    return espnow_connected;
}

void set_var_espnow_connected(bool value) {
    espnow_connected = value;
}

const char* get_var_wx_rcv_time() {
    // Note: wx_rcv_time is protected by hotPacketMutex
    // For UI getter, we skip mutex to avoid blocking LVGL refresh
    // Data race is acceptable here as String reads are atomic enough for display
    return wx_rcv_time.c_str();
}

void set_var_wx_rcv_time(const char* value) {
    wx_rcv_time = String(value);
}

const char* get_var_cur_temp() {
    // Prefer ESP-NOW air temperature if available, fallback to Meshtastic weather data
    static String tempStr;

    if (airTemperature != -99) {
        // ESP-NOW temperature is available (has real-time data)
        tempStr = String((int)round(airTemperature));
        return tempStr.c_str();
    } else {
        // Fallback to Meshtastic weather data
        // Note: cur_temp is protected by hotPacketMutex in parseWeatherData
        // For UI getter, we skip mutex to avoid blocking LVGL refresh
        return cur_temp.c_str();
    }
}

void set_var_cur_temp(const char* value) {
    cur_temp = String(value);
}

const char* get_var_fcast_hr1() {
    return fcast_hr1.c_str();
}

void set_var_fcast_hr1(const char* value) {
    fcast_hr1 = String(value);
}

const char* get_var_fcast_glyph1() {
    return fcast_glyph1.c_str();
}

void set_var_fcast_glyph1(const char* value) {
    fcast_glyph1 = String(value);
}

const char* get_var_fcast_temp1() {
    return fcast_temp1.c_str();
}

void set_var_fcast_temp1(const char* value) {
    fcast_temp1 = String(value);
}

const char* get_var_fcast_precip1() {
    return fcast_precip1.c_str();
}

void set_var_fcast_precip1(const char* value) {
    fcast_precip1 = String(value);
}

const char* get_var_fcast_hr2() {
    return fcast_hr2.c_str();
}

void set_var_fcast_hr2(const char* value) {
    fcast_hr2 = String(value);
}

const char* get_var_fcast_glyph2() {
    return fcast_glyph2.c_str();
}

void set_var_fcast_glyph2(const char* value) {
    fcast_glyph2 = String(value);
}

const char* get_var_fcast_temp2() {
    return fcast_temp2.c_str();
}

void set_var_fcast_temp2(const char* value) {
    fcast_temp2 = String(value);
}

const char* get_var_fcast_precip2() {
    return fcast_precip2.c_str();
}

void set_var_fcast_precip2(const char* value) {
    fcast_precip2 = String(value);
}

const char* get_var_fcast_hr3() {
    return fcast_hr3.c_str();
}

void set_var_fcast_hr3(const char* value) {
    fcast_hr3 = String(value);
}

const char* get_var_fcast_glyph3() {
    return fcast_glyph3.c_str();
}

void set_var_fcast_glyph3(const char* value) {
    fcast_glyph3 = String(value);
}

const char* get_var_fcast_temp3() {
    return fcast_temp3.c_str();
}

void set_var_fcast_temp3(const char* value) {
    fcast_temp3 = String(value);
}

const char* get_var_fcast_precip3() {
    return fcast_precip3.c_str();
}

void set_var_fcast_precip3(const char* value) {
    fcast_precip3 = String(value);
}

const char* get_var_fcast_hr4() {
    return fcast_hr4.c_str();
}

void set_var_fcast_hr4(const char* value) {
    fcast_hr4 = String(value);
}

const char* get_var_fcast_glyph4() {
    return fcast_glyph4.c_str();
}

void set_var_fcast_glyph4(const char* value) {
    fcast_glyph4 = String(value);
}

const char* get_var_fcast_temp4() {
    return fcast_temp4.c_str();
}

void set_var_fcast_temp4(const char* value) {
    fcast_temp4 = String(value);
}

const char* get_var_fcast_precip4() {
    return fcast_precip4.c_str();
}

void set_var_fcast_precip4(const char* value) {
    fcast_precip4 = String(value);
}

const char* get_var_np_rcv_time() {
    // Note: np_rcv_time is protected by hotPacketMutex
    // For UI getter, we skip mutex to avoid blocking LVGL refresh
    return np_rcv_time.c_str();
}

void set_var_np_rcv_time(const char* value) {
    np_rcv_time = String(value);
}


const char* get_var_espnow_status() {
    return espnow_status.c_str();
}

void set_var_espnow_status(const char* value) {
    espnow_status = String(value);
}

const char* get_var_espnow_last_received() {
    return espnow_last_received.c_str();
}

void set_var_espnow_last_received(const char* value) {
    espnow_last_received = String(value);
}

const char* get_var_gcm_node_id() {
    return gcm_node_id.c_str();
}

void set_var_gcm_node_id(const char* value) {
    gcm_node_id = String(value);
}

int32_t get_var_screen_inactivity_countdown() {
    return screen_inactivity_countdown;
}

void set_var_screen_inactivity_countdown(int32_t value) {
    screen_inactivity_countdown = value;
}

bool get_var_flip_screen() {
    return flip_screen;
}

void set_var_flip_screen(bool value) {
    if (flip_screen != value) {
        Serial.print("flip_screen changed from ");
        Serial.print(flip_screen ? "true" : "false");
        Serial.print(" to ");
        Serial.println(value ? "true" : "false");

        flip_screen = value;

        // Update display rotation immediately
        updateDisplayRotation();

        // Queue the preference write to save to EEPROM
        queuePreferenceWrite("flip_screen", value);
    }
}

int32_t get_var_speaker_volume() {
    return speaker_volume;
}

void set_var_speaker_volume(int32_t value) {
    speaker_volume = value;
}

const char* get_var_odometer() {
    return odometer.c_str();
}

void set_var_odometer(const char* value) {
    odometer = String(value);
}

const char* get_var_trip_odometer() {
    return trip_odometer.c_str();
}

void set_var_trip_odometer(const char* value) {
    trip_odometer = String(value);
}

/**
 * Hours Since Service - Storage Format Documentation
 *
 * IMPORTANT: hrs_since_svc is stored EVERYWHERE as tenths of hours (0.1 hour = 6 minute resolution)
 *
 * Storage Locations (all as tenths):
 *   - EEPROM: int32_t stored as tenths (e.g., 15 = 1.5 hours)
 *   - Global variable hrs_since_svc: int32_t tenths
 *   - GPS task accumulation: converts to/from seconds for calculation
 *
 * Conversions:
 *   - UI Display: hrs_since_svc / 10 → whole hours
 *   - UI Input: whole hours * 10 → hrs_since_svc
 *   - GPS Init: hrs_since_svc * 360 → accumSeconds (for tracking)
 *   - GPS Accumulate: accumSeconds / 360 → hrs_since_svc
 *
 * Example: 125 tenths = 12.5 hours = 12 hours 30 minutes
 */

int32_t get_var_hrs_since_svc() {
    // Convert tenths of hours to whole hours for UI display
    return hrs_since_svc / 10;
}

void set_var_hrs_since_svc(int32_t value) {
    // Convert whole hours from UI to tenths for internal storage
    hrs_since_svc = value * 10;
    // Save immediately when user resets from UI (typically to 0 after service)
    queuePreferenceWrite("hrs_since_svc", hrs_since_svc);
}

int32_t get_var_svc_interval_hrs() {
    return svc_interval_hrs;
}

void set_var_svc_interval_hrs(int32_t value) {
    svc_interval_hrs = value;
}

float get_var_accum_distance() {
    return accumDistance;
}

void set_var_accum_distance(float value) {
    accumDistance = value;
    // Update formatted display string
    odometer = String(accumDistance, 1);
}

float get_var_trip_distance() {
    return tripDistance;
}

void set_var_trip_distance(float value) {
    tripDistance = value;
    // Update formatted display string
    trip_odometer = String(tripDistance, 1);
}

bool get_var_reset_preferences() {
    return reset_preferences;
}

void set_var_reset_preferences(bool value) {
    reset_preferences = value;
}

bool get_var_espnow_pair_gci() {
    return espnow_pair_gci;
}

void set_var_espnow_pair_gci(bool value) {
    espnow_pair_gci = value;
}

float get_var_temperature_adj() {
    return temperature_adj;
}

void set_var_temperature_adj(float value) {
    temperature_adj = value;
}

bool get_var_reboot_meshtastic() {
    return reboot_meshtastic;
}

void set_var_reboot_meshtastic(bool value) {
    reboot_meshtastic = value;
}

const char* get_var_text_message() {
    return text_message.c_str();
}

void set_var_text_message(const char* value) {
    text_message = String(value);
}

} // extern "C"