#include "get_set_vars.h"
#include <Arduino.h>

// String variable definitions
String cur_date;
String heading;
String hhmmss_str;
String hhmm_str;
String am_pm_str;
String sats_hdop;
String version;
String cyd_mac_addr;
String espnow_mac_addr;
String wx_rcv_time;
String cur_temp;
String fcast_hr1;
String fcast_glyph1;
String fcast_precip1;
String fcast_hr2;
String fcast_glyph2;
String fcast_precip2;
String fcast_hr3;
String fcast_glyph3;
String fcast_precip3;
String fcast_hr4;
String fcast_glyph4;
String fcast_precip4;
String np_rcv_time;
String espnow_status;
String espnow_last_received;

// Numeric variable definitions
int32_t avg_speed = 0;
float max_hdop = 2.5;
int32_t day_backlight = 10;
int32_t night_backlight = 5;
bool manual_reboot = false;
bool new_rx_data_flag = false;
bool mesh_comm = true;
int32_t screen_inactivity_countdown = 0;

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

float get_var_max_hdop() {
    return max_hdop;
}

void set_var_max_hdop(float value) {
    max_hdop = value;
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

const char* get_var_espnow_mac_addr() {
    return espnow_mac_addr.c_str();
}

void set_var_espnow_mac_addr(const char* value) {
    espnow_mac_addr = String(value);
}

bool get_var_mesh_comm() {
    return mesh_comm;
}

void set_var_mesh_comm(bool value) {
    mesh_comm = value;
}

const char* get_var_wx_rcv_time() {
    return wx_rcv_time.c_str();
}

void set_var_wx_rcv_time(const char* value) {
    wx_rcv_time = String(value);
}

const char* get_var_cur_temp() {
    return cur_temp.c_str();
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

const char* get_var_fcast_precip4() {
    return fcast_precip4.c_str();
}

void set_var_fcast_precip4(const char* value) {
    fcast_precip4 = String(value);
}

const char* get_var_np_rcv_time() {
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

int32_t get_var_screen_inactivity_countdown() {
    return screen_inactivity_countdown;
}

void set_var_screen_inactivity_countdown(int32_t value) {
    screen_inactivity_countdown = value;
}


} // extern "C"