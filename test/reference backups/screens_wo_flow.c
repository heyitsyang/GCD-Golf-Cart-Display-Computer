#include <string.h>

#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"

#include <string.h>

objects_t objects;
lv_obj_t *tick_value_change_obj;
uint32_t active_theme_index = 0;


/* ADD EVENT CALLBACK PROCESSING BELOW */

// Gesture callback
int currentScreen = 1;         //Main Screen=1 keep track of which screen we are in.
int numberOfHorizSwipeScreens = 4;  //MUST change to number of screens

void gesture_event(lv_event_t *e) {
  lv_obj_t *screen = lv_event_get_current_target(e);
  lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());

  switch (dir) {
    case LV_DIR_LEFT:
      //Go towards the last screen
      if (currentScreen < numberOfHorizSwipeScreens) {
        loadScreen(currentScreen + 1);
        currentScreen = currentScreen + 1;
      } else {
        // go to first screen
        currentScreen = SCREEN_ID_HOME;  //screen 1
        loadScreen(currentScreen);
      }
      break;
    case LV_DIR_RIGHT:
      //Go towards the Main/First screen
      if (currentScreen > 1) {
        loadScreen(currentScreen - 1);
        currentScreen = currentScreen - 1;
      } else {
        // wrap to last
        currentScreen = numberOfHorizSwipeScreens;
        loadScreen(currentScreen);
      }
      break;
    case LV_DIR_TOP:
      switch(currentScreen) {
        case SCREEN_ID_HOME:
          currentScreen = SCREEN_ID_MESHTASTIC;
          loadScreen(currentScreen);
          break;
        case SCREEN_ID_PIN_ENTRY:
          currentScreen = SCREEN_ID_HOME;
          loadScreen(currentScreen);
          break;
        case SCREEN_ID_SETTINGS:
          currentScreen = SCREEN_ID_DIAGNOSTICS;
          loadScreen(currentScreen);
          break;
      }
      break;
    case LV_DIR_BOTTOM:
      switch(currentScreen) {
        case SCREEN_ID_HOME:
          currentScreen = SCREEN_ID_PIN_ENTRY;
          loadScreen(currentScreen);
          break;
        case SCREEN_ID_MESHTASTIC:
          currentScreen = SCREEN_ID_HOME;
          loadScreen(currentScreen);
          break;
        case SCREEN_ID_DIAGNOSTICS:
          currentScreen = SCREEN_ID_SETTINGS;
          loadScreen(currentScreen);
          break;
      }
    break;
  }
}



/* ADD EVENT CALLBACK HOOKS INTO FOLLOWING FUNCTIONS */
void create_screen_home() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.home = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 320);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_src(obj, &img_blk_triangles_2d, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff9e9e9e), LV_PART_SCROLLBAR | LV_STATE_PRESSED);
    lv_obj_add_event_cb(obj, gesture_event, LV_EVENT_GESTURE, NULL);    // ADD CALLBACK
    {
        lv_obj_t *parent_obj = obj;
        {
            // lbl_heading_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_heading_value = obj;
            lv_obj_set_pos(obj, 0, 19);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_rem_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "NNW");
        }
        {
            // lbl_heading_text
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_heading_text = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "HEADING");
        }
        {
            // lbl_speed_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_speed_value = obj;
            lv_obj_set_pos(obj, 421, 19);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_rem_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "00");
        }
        {
            // lbl_MPH_text
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_mph_text = obj;
            lv_obj_set_pos(obj, 438, 0);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "MPH");
        }
        {
            // lbl_date_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_date_value = obj;
            lv_obj_set_pos(obj, 171, 162);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Tue, Dec 25");
        }
        {
            // lbl_time_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_time_value = obj;
            lv_obj_set_pos(obj, 108, 107);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_rem_58, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "12:00 PM");
        }
        {
            // bar_fuel
            lv_obj_t *obj = lv_bar_create(parent_obj);
            objects.bar_fuel = obj;
            lv_obj_set_pos(obj, 453, 102);
            lv_obj_set_size(obj, 19, 147);
            lv_bar_set_value(obj, 25, LV_ANIM_OFF);
            lv_obj_set_style_outline_color(obj, lv_color_hex(0xff1d5d95), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_outline_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xfff32121), LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 6, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        }
        {
            // lbl_fuel_empty
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_fuel_empty = obj;
            lv_obj_set_pos(obj, 456, 231);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "E");
        }
        {
            // lbl_fuel_full
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_fuel_full = obj;
            lv_obj_set_pos(obj, 457, 101);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "F");
        }
    }
    
    tick_screen_home();
}

void tick_screen_home() {
}

void create_screen_weather() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.weather = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 320);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(obj, gesture_event, LV_EVENT_GESTURE, NULL);    // ADD CALLBACK
    {
        lv_obj_t *parent_obj = obj;
        {
            // lbl_humidity_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_humidity_value = obj;
            lv_obj_set_pos(obj, 372, 19);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "100%");
        }
        {
            // lbl_temperature_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_temperature_value = obj;
            lv_obj_set_pos(obj, 0, 19);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "100°F");
        }
        {
            // lbl_temperature_text
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_temperature_text = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "TEMPERATURE");
        }
        {
            // lbl_humidity_text
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_humidity_text = obj;
            lv_obj_set_pos(obj, 403, 0);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "HUMIDITY");
        }
    }
    
    tick_screen_weather();
}

void tick_screen_weather() {
}

void create_screen_maintenance() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.maintenance = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 320);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(obj, gesture_event, LV_EVENT_GESTURE, NULL);    // ADD CALLBACK
    {
        lv_obj_t *parent_obj = obj;
        {
            // lbl_maintenance_title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_maintenance_title = obj;
            lv_obj_set_pos(obj, 155, 0);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Maintentance");
        }
    }
    
    tick_screen_maintenance();
}

void tick_screen_maintenance() {
}

void create_screen_settings() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.settings = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 320);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(obj, gesture_event, LV_EVENT_GESTURE, NULL);    // ADD CALLBACK
    {
        lv_obj_t *parent_obj = obj;
        {
            // lbl_settings_title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_settings_title = obj;
            lv_obj_set_pos(obj, 189, 0);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Settings");
        }
    }
    
    tick_screen_settings();
}

void tick_screen_settings() {
}

void create_screen_diagnostics() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.diagnostics = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 320);
    add_style_page_black_text_white(obj);
    lv_obj_add_event_cb(obj, gesture_event, LV_EVENT_GESTURE, NULL);    // ADD CALLBACK
    {
        lv_obj_t *parent_obj = obj;
        {
            // lbl_diagnostics_title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_diagnostics_title = obj;
            lv_obj_set_pos(obj, 168, 0);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Diagnostics");
        }
    }
    
    tick_screen_diagnostics();
}

void tick_screen_diagnostics() {
}

void create_screen_meshtastic() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.meshtastic = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 320);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(obj, gesture_event, LV_EVENT_GESTURE, NULL);    // ADD CALLBACK
    {
        lv_obj_t *parent_obj = obj;
        {
            // lbl_meshastic_title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_meshastic_title = obj;
            lv_obj_set_pos(obj, 174, 0);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Meshtastic");
        }
    }
    
    tick_screen_meshtastic();
}

void tick_screen_meshtastic() {
}

void create_screen_pin_entry() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.pin_entry = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 320);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff68d284), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(obj, gesture_event, LV_EVENT_GESTURE, NULL);    // ADD CALLBACK
    {
        lv_obj_t *parent_obj = obj;
        {
            // lbl_enter_pin_title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_enter_pin_title = obj;
            lv_obj_set_pos(obj, 182, 0);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Enter PIN");
        }
        {
            // btn_matrix_PIN
            lv_obj_t *obj = lv_buttonmatrix_create(parent_obj);
            objects.btn_matrix_pin = obj;
            lv_obj_set_pos(obj, 0, 87);
            lv_obj_set_size(obj, LV_PCT(100), 233);
            static const char *map[16] = {
                "1",
                "2",
                "3",
                "\n",
                "4",
                "5",
                "6",
                "\n",
                "7",
                "8",
                "9",
                "\n",
                "Exit",
                "0",
                "Submit",
                NULL,
            };
            static lv_buttonmatrix_ctrl_t ctrl_map[12] = {
                1 | LV_BUTTONMATRIX_CTRL_NO_REPEAT,
                1 | LV_BUTTONMATRIX_CTRL_NO_REPEAT,
                1 | LV_BUTTONMATRIX_CTRL_NO_REPEAT,
                1 | LV_BUTTONMATRIX_CTRL_NO_REPEAT,
                1 | LV_BUTTONMATRIX_CTRL_NO_REPEAT,
                1 | LV_BUTTONMATRIX_CTRL_NO_REPEAT,
                1 | LV_BUTTONMATRIX_CTRL_NO_REPEAT,
                1 | LV_BUTTONMATRIX_CTRL_NO_REPEAT,
                1 | LV_BUTTONMATRIX_CTRL_NO_REPEAT,
                1 | LV_BUTTONMATRIX_CTRL_NO_REPEAT,
                1 | LV_BUTTONMATRIX_CTRL_NO_REPEAT,
                1 | LV_BUTTONMATRIX_CTRL_NO_REPEAT,
            };
            lv_buttonmatrix_set_map(obj, map);
            lv_buttonmatrix_set_ctrl_map(obj, ctrl_map);
            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                static lv_coord_t dsc[] = {LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            {
                static lv_coord_t dsc[] = {LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_START, LV_PART_ITEMS | LV_STATE_DEFAULT);
            {
                static lv_coord_t dsc[] = {LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_ITEMS | LV_STATE_DEFAULT);
            }
            {
                static lv_coord_t dsc[] = {LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_ITEMS | LV_STATE_DEFAULT);
            }
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xff68d284), LV_PART_ITEMS | LV_STATE_PRESSED);
        }
        {
            // lbl_show_PIN_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_show_pin_value = obj;
            lv_obj_set_pos(obj, 187, 43);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "123456");
        }
    }
    
    tick_screen_pin_entry();
}

void tick_screen_pin_entry() {
}



typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_home,
    tick_screen_weather,
    tick_screen_maintenance,
    tick_screen_settings,
    tick_screen_diagnostics,
    tick_screen_meshtastic,
    tick_screen_pin_entry,
};
void tick_screen(int screen_index) {
    tick_screen_funcs[screen_index]();
}
void tick_screen_by_id(enum ScreensEnum screenId) {
    tick_screen_funcs[screenId - 1]();
}

void create_screens() {
    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    
    create_screen_home();
    create_screen_weather();
    create_screen_maintenance();
    create_screen_settings();
    create_screen_diagnostics();
    create_screen_meshtastic();
    create_screen_pin_entry();
}
