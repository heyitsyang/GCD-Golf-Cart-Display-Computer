
/*
 * functions required to get & set variables generated from EEZ Studio
 */


#include <string.h>
#include "ui/vars.h"
//#include "ui/structs.h"

String cur_date;
extern "C" const char *get_var_cur_date() {
    return cur_date.c_str();
}

extern "C" void set_var_cur_date(const char *value) {
    cur_date = value;
}


String heading;
extern "C" const char *get_var_heading() {
    return heading.c_str();
}

extern "C" void set_var_heading(const char *value) {
    heading = value;
}


String hhmmss_str;
extern "C" const char *get_var_hhmmss_str() {
    return hhmmss_str.c_str();
}

extern "C" void set_var_hhmmss_str(const char *value) {
    hhmmss_str = value;
}


String hhmm_str;
extern "C" const char *get_var_hhmm_str() {
    return hhmm_str.c_str();
}

extern "C" void set_var_hhmm_str(const char *value) {
    hhmm_str = value;
}


String am_pm_str;
extern "C" const char *get_var_am_pm_str() {
    return am_pm_str.c_str();
}

extern "C" void set_var_am_pm_str(const char *value) {
    am_pm_str = value;
}


String sats_hdop;
extern "C" const char *get_var_sats_hdop() {
    return sats_hdop.c_str();
}

extern "C" void set_var_sats_hdop(const char *value) {
    sats_hdop = value;
}


int32_t avg_speed;
int32_t get_var_avg_speed() {
    return avg_speed;
}

void set_var_avg_speed(int32_t value) {
    avg_speed = value;
}

float max_hdop;
float get_var_max_hdop() {
    return max_hdop;
}

void set_var_max_hdop(float value) {
    max_hdop = value;
}

String version;
extern "C" const char *get_var_version() {
    return version.c_str();
}

extern "C" void set_var_version(const char *value) {
    version = value;
}

int day_backlight;
int get_var_cyd_day_backlight() {
    return day_backlight;
}

void set_var_cyd_day_backlight(int value) {
    day_backlight = value;
}

int night_backlight;
int get_var_cyd_night_backlight() {
    return night_backlight;
}

void set_var_cyd_night_backlight(int value) {
    night_backlight = value;
}

String cyd_mac_addr;
extern "C" const char *get_var_cyd_mac_addr() {
    return cyd_mac_addr.c_str();
}

extern "C" void set_var_cyd_mac_addr(const char *value) {
    cyd_mac_addr = value;
}

bool manual_reboot;

extern "C" bool get_var_manual_reboot() {
    return manual_reboot;
}

extern "C" void set_var_manual_reboot(bool value) {
    manual_reboot = value;
}

String espnow_mac_addr;

extern "C" const char *get_var_espnow_mac_addr() {
    return espnow_mac_addr.c_str();
}

extern "C" void set_var_espnow_mac_addr(const char *value) {
    espnow_mac_addr = value;
}

bool mesh_comm;

bool get_var_mesh_comm() {
    return mesh_comm;
}

void set_var_mesh_comm(bool value) {
    mesh_comm = value;
}

String wx_rcv_time;

extern "C" const char *get_var_wx_rcv_time() {
    return wx_rcv_time.c_str();
}

extern "C" void set_var_wx_rcv_time(const char *value) {
    wx_rcv_time = value;
}


// Weather globals

String cur_temp;

extern "C" const char *get_var_cur_temp() {
    return cur_temp.c_str();
}

extern "C" void set_var_cur_temp(const char *value) {
    cur_temp = value;
}


// hour 1 data
String fcast_hr1;

extern "C" const char *get_var_fcast_hr1() {
    return fcast_hr1.c_str();
}

extern "C" void set_var_fcast_hr1(const char *value) {
    fcast_hr1 = value;
}


String fcast_glyph1;

extern "C" const char *get_var_fcast_glyph1() {
    return fcast_glyph1.c_str();
}

extern "C" void set_var_fcast_glyph1(const char *value) {
    fcast_glyph1 = value;
}


String fcast_precip1;

extern "C" const char *get_var_fcast_precip1() {
    return fcast_precip1.c_str();
}

extern "C" void set_var_fcast_precip1(const char *value) {
    fcast_precip1 = value;
}


// hour 2 data
String fcast_hr2;

extern "C" const char *get_var_fcast_hr2() {
    return fcast_hr2.c_str();
}

extern "C" void set_var_fcast_hr2(const char *value) {
    fcast_hr2 = value;
}


String fcast_glyph2;

extern "C" const char *get_var_fcast_glyph2() {
    return fcast_glyph2.c_str();
}

extern "C" void set_var_fcast_glyph2(const char *value) {
    fcast_glyph2 = value;
}


String fcast_precip2;

extern "C" const char *get_var_fcast_precip2() {
    return fcast_precip2.c_str();
}

extern "C" void set_var_fcast_precip2(const char *value) {
    fcast_precip2 = value;
}


// hour 3 data
String fcast_hr3;

extern "C" const char *get_var_fcast_hr3() {
    return fcast_hr3.c_str();
}

extern "C" void set_var_fcast_hr3(const char *value) {
    fcast_hr3 = value;
}


String fcast_glyph3;

extern "C" const char *get_var_fcast_glyph3() {
    return fcast_glyph3.c_str();
}

extern "C" void set_var_fcast_glyph3(const char *value) {
    fcast_glyph3 = value;
}


String fcast_precip3;

extern "C" const char *get_var_fcast_precip3() {
    return fcast_precip3.c_str();
}

extern "C" void set_var_fcast_precip3(const char *value) {
    fcast_precip3 = value;
}


// hour 4 data
String fcast_hr4;

extern "C" const char *get_var_fcast_hr4() {
    return fcast_hr4.c_str();
}

extern "C" void set_var_fcast_hr4(const char *value) {
    fcast_hr4 = value;
}


String fcast_glyph4;

extern "C" const char *get_var_fcast_glyph4() {
    return fcast_glyph4.c_str();
}

extern "C" void set_var_fcast_glyph4(const char *value) {
    fcast_glyph4 = value;
}


String fcast_precip4;

extern "C" const char *get_var_fcast_precip4() {
    return fcast_precip4.c_str();
}

extern "C" void set_var_fcast_precip4(const char *value) {
    fcast_precip4 = value;
}
