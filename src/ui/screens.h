#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *home;
    lv_obj_t *weather;
    lv_obj_t *meshtastic;
    lv_obj_t *maintenance;
    lv_obj_t *settings;
    lv_obj_t *pin_entry;
    lv_obj_t *diagnostics;
    lv_obj_t *lbl_heading_value;
    lv_obj_t *lbl_speed_value;
    lv_obj_t *lbl_mph_text;
    lv_obj_t *lbl_date_value;
    lv_obj_t *lbl_heading_text;
    lv_obj_t *lbl_time_value;
    lv_obj_t *bar_fuel;
    lv_obj_t *lbl_fuel_empty;
    lv_obj_t *lbl_fuel_full;
    lv_obj_t *obj0;
    lv_obj_t *obj1;
    lv_obj_t *lbl_meshastic_title;
    lv_obj_t *lbl_maintenance_title;
    lv_obj_t *lbl_settings_title;
    lv_obj_t *lbl_pin_start_title;
    lv_obj_t *btn_matrix_pin;
    lv_obj_t *lbl_show_pin;
    lv_obj_t *lbl_diagnostics_title;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_HOME = 1,
    SCREEN_ID_WEATHER = 2,
    SCREEN_ID_MESHTASTIC = 3,
    SCREEN_ID_MAINTENANCE = 4,
    SCREEN_ID_SETTINGS = 5,
    SCREEN_ID_PIN_ENTRY = 6,
    SCREEN_ID_DIAGNOSTICS = 7,
};

void create_screen_home();
void tick_screen_home();

void create_screen_weather();
void tick_screen_weather();

void create_screen_meshtastic();
void tick_screen_meshtastic();

void create_screen_maintenance();
void tick_screen_maintenance();

void create_screen_settings();
void tick_screen_settings();

void create_screen_pin_entry();
void tick_screen_pin_entry();

void create_screen_diagnostics();
void tick_screen_diagnostics();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/