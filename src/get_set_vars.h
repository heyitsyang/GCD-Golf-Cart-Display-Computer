
/*
 * functions required to get & set variables generated from EEZ Studio
 */


#include <string.h>
#include "ui/vars.h"

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


String hhmmss_t;
extern "C" const char *get_var_hhmmss_t() {
    return hhmmss_t.c_str();
}

extern "C" void set_var_hhmmss_t(const char *value) {
    hhmmss_t = value;
}


String hhmm_t;
extern "C" const char *get_var_hhmm_t() {
    return hhmm_t.c_str();
}

extern "C" void set_var_hhmm_t(const char *value) {
    hhmm_t = value;
}


String str_am_pm;
extern "C" const char *get_var_str_am_pm() {
    return str_am_pm.c_str();
}

extern "C" void set_var_str_am_pm(const char *value) {
    str_am_pm = value;
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
