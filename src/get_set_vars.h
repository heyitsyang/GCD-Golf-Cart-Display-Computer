#ifndef GET_SET_VARS_H
#define GET_SET_VARS_H

#ifdef __cplusplus
#include <Arduino.h>
#else
#include <stdint.h>
#include <stdbool.h>
#endif

// Function declarations for C compatibility
#ifdef __cplusplus
extern "C" {
#endif

// All function declarations
const char* get_var_cur_date();
void set_var_cur_date(const char* value);

const char* get_var_heading();
void set_var_heading(const char* value);

const char* get_var_hhmmss_str();
void set_var_hhmmss_str(const char* value);

const char* get_var_hhmm_str();
void set_var_hhmm_str(const char* value);

const char* get_var_am_pm_str();
void set_var_am_pm_str(const char* value);

const char* get_var_sats_hdop();
void set_var_sats_hdop(const char* value);

int32_t get_var_avg_speed();
void set_var_avg_speed(int32_t value);

float get_var_max_hdop();
void set_var_max_hdop(float value);

const char* get_var_version();
void set_var_version(const char* value);

int32_t get_var_cyd_day_backlight();
void set_var_cyd_day_backlight(int32_t value);

int32_t get_var_cyd_night_backlight();
void set_var_cyd_night_backlight(int32_t value);

const char* get_var_cyd_mac_addr();
void set_var_cyd_mac_addr(const char* value);

bool get_var_manual_reboot();
void set_var_manual_reboot(bool value);

bool get_var_new_rx_data_flag();
void set_var_new_rx_data_flag(bool value);

const char* get_var_espnow_mac_addr();
void set_var_espnow_mac_addr(const char* value);

bool get_var_mesh_serial_enabled();
void set_var_mesh_serial_enabled(bool value);

bool get_var_espnow_connected();
void set_var_espnow_connected(bool value);

const char* get_var_wx_rcv_time();
void set_var_wx_rcv_time(const char* value);

const char* get_var_cur_temp();
void set_var_cur_temp(const char* value);

const char* get_var_fcast_hr1();
void set_var_fcast_hr1(const char* value);

const char* get_var_fcast_glyph1();
void set_var_fcast_glyph1(const char* value);

const char* get_var_fcast_precip1();
void set_var_fcast_precip1(const char* value);

const char* get_var_fcast_hr2();
void set_var_fcast_hr2(const char* value);

const char* get_var_fcast_glyph2();
void set_var_fcast_glyph2(const char* value);

const char* get_var_fcast_precip2();
void set_var_fcast_precip2(const char* value);

const char* get_var_fcast_hr3();
void set_var_fcast_hr3(const char* value);

const char* get_var_fcast_glyph3();
void set_var_fcast_glyph3(const char* value);

const char* get_var_fcast_precip3();
void set_var_fcast_precip3(const char* value);

const char* get_var_fcast_hr4();
void set_var_fcast_hr4(const char* value);

const char* get_var_fcast_glyph4();
void set_var_fcast_glyph4(const char* value);

const char* get_var_fcast_precip4();
void set_var_fcast_precip4(const char* value);

const char* get_var_np_rcv_time();
void set_var_np_rcv_time(const char* value);

const char* get_var_espnow_status();
void set_var_espnow_status(const char* value);

const char* get_var_espnow_last_received();
void set_var_espnow_last_received(const char* value);

int32_t get_var_screen_inactivity_countdown();
void set_var_screen_inactivity_countdown(int32_t value);

bool get_var_flip_screen();
void set_var_flip_screen(bool value);

int32_t get_var_speaker_volume();
void set_var_speaker_volume(int32_t value);

bool get_var_reset_preferences();
void set_var_reset_preferences(bool value);

bool get_var_espnow_pair_gci();
void set_var_espnow_pair_gci(bool value);

#ifdef __cplusplus
}
#endif

// Variable declarations for C++ only
#ifdef __cplusplus

// String variables
extern String cur_date;
extern String heading;
extern String hhmmss_str;
extern String hhmm_str;
extern String am_pm_str;
extern String sats_hdop;
extern String version;
extern String cyd_mac_addr;
extern String espnow_mac_addr;
extern String wx_rcv_time;
extern String cur_temp;
extern String fcast_hr1;
extern String fcast_glyph1;
extern String fcast_precip1;
extern String fcast_hr2;
extern String fcast_glyph2;
extern String fcast_precip2;
extern String fcast_hr3;
extern String fcast_glyph3;
extern String fcast_precip3;
extern String fcast_hr4;
extern String fcast_glyph4;
extern String fcast_precip4;
extern String np_rcv_time;
extern String espnow_status;
extern String espnow_last_received;

// Numeric variables
extern int32_t avg_speed;
extern float max_hdop;
extern int32_t day_backlight;
extern int32_t night_backlight;
extern bool manual_reboot;
extern bool new_rx_data_flag;
extern bool mesh_serial_enabled;
extern bool espnow_connected;
extern int32_t screen_inactivity_countdown;
extern bool flip_screen;
extern int32_t speaker_volume;
extern bool reset_preferences;
extern bool espnow_pair_gci;


#endif // __cplusplus

#endif // GET_SET_VARS_H