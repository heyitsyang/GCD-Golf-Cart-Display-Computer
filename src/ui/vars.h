#ifndef EEZ_LVGL_UI_VARS_H
#define EEZ_LVGL_UI_VARS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// enum declarations



// Flow global variables

enum FlowGlobalVariables {
    FLOW_GLOBAL_VARIABLE_NONE
};

// Native global variables

extern const char *get_var_heading();
extern void set_var_heading(const char *value);
extern const char *get_var_hhmm_t();
extern void set_var_hhmm_t(const char *value);
extern int32_t get_var_speed();
extern void set_var_speed(int32_t value);
extern const char *get_var_cur_date();
extern void set_var_cur_date(const char *value);
extern const char *get_var_hhmmss_t();
extern void set_var_hhmmss_t(const char *value);
extern const char *get_var_str_am_pm();
extern void set_var_str_am_pm(const char *value);
extern const char *get_var_sats_hdop();
extern void set_var_sats_hdop(const char *value);


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_VARS_H*/