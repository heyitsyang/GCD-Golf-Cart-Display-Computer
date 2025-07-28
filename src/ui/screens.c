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

// ADD GESTURE CALLBACK
int currentScreen = 1;         //Main Screen=1 keep track of which screen we are in.
int numberOfSwipeScreens = 5;  //MUST change to number of screens

void gesture_event_cb(lv_event_t *e) {
  lv_obj_t *screen = lv_event_get_current_target(e);
  lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());

  
  switch (dir) {
    case LV_DIR_LEFT:
      //Go towards the last screen
      if (currentScreen < numberOfSwipeScreens) {
        loadScreen(currentScreen + 1);
        currentScreen = currentScreen + 1;
      } else {
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
        currentScreen = numberOfSwipeScreens;
        loadScreen(currentScreen);
      }
      break;
    case LV_DIR_TOP:
      //Go to set screen
      currentScreen = SCREEN_ID_MESHTASTIC;   // screen 3
      loadScreen(currentScreen);
      break;
    case LV_DIR_BOTTOM:
      //Go to set screen
      currentScreen = SCREEN_ID_PIN_ENTRY;   // screen 6
      loadScreen(currentScreen);
      break;
  }
}

void create_screen_home() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.home = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff9e9e9e), LV_PART_SCROLLBAR | LV_STATE_PRESSED);
    lv_obj_add_event_cb(obj, gesture_event_cb, LV_EVENT_GESTURE, NULL);    // ADD CALLBACK
    {
        lv_obj_t *parent_obj = obj;
        {
            // lbl_heading_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_heading_value = obj;
            lv_obj_set_pos(obj, 0, 13);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "NNW");
        }
        {
            // lbl_speed_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_speed_value = obj;
            lv_obj_set_pos(obj, 271, 13);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "00");
        }
        {
            // lbl_MPH_text
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_mph_text = obj;
            lv_obj_set_pos(obj, 278, 0);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "MPH");
        }
        {
            // lbl_date_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_date_value = obj;
            lv_obj_set_pos(obj, 82, 123);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Tue, Dec 25");
        }
        {
            // lbl_heading_text
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_heading_text = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "HEADING");
        }
        {
            // lbl_time_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_time_value = obj;
            lv_obj_set_pos(obj, 71, 83);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_36, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "12:00 PM");
        }
        {
            // bar_fuel
            lv_obj_t *obj = lv_bar_create(parent_obj);
            objects.bar_fuel = obj;
            lv_obj_set_pos(obj, 278, 67);
            lv_obj_set_size(obj, 26, 122);
            lv_bar_set_value(obj, 25, LV_ANIM_OFF);
        }
        {
            // lbl_fuel_empty
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_fuel_empty = obj;
            lv_obj_set_pos(obj, 286, 189);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "E");
        }
        {
            // lbl_fuel_full
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_fuel_full = obj;
            lv_obj_set_pos(obj, 286, 49);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
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
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(obj, gesture_event_cb, LV_EVENT_GESTURE, NULL);    // ADD CALLBACK
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj0 = obj;
            lv_obj_set_pos(obj, 217, 0);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "100%");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj1 = obj;
            lv_obj_set_pos(obj, 1, 0);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "100°");
        }
    }
    
    tick_screen_weather();
}

void tick_screen_weather() {
}

void create_screen_meshtastic() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.meshtastic = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(obj, gesture_event_cb, LV_EVENT_GESTURE, NULL);    // ADD CALLBACK
    {
        lv_obj_t *parent_obj = obj;
        {
            // lbl_meshastic_title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_meshastic_title = obj;
            lv_obj_set_pos(obj, 105, 0);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Meshtastic");
        }
    }
    
    tick_screen_meshtastic();
}

void tick_screen_meshtastic() {
}

void create_screen_maintenance() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.maintenance = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(obj, gesture_event_cb, LV_EVENT_GESTURE, NULL);    // ADD CALLBACK
    {
        lv_obj_t *parent_obj = obj;
        {
            // lbl_maintenance_title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_maintenance_title = obj;
            lv_obj_set_pos(obj, 105, 1);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Maintenance");
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
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(obj, gesture_event_cb, LV_EVENT_GESTURE, NULL);    // ADD CALLBACK
    {
        lv_obj_t *parent_obj = obj;
        {
            // lbl_settings_title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_settings_title = obj;
            lv_obj_set_pos(obj, 105, 1);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Settings");
        }
    }
    
    tick_screen_settings();
}

void tick_screen_settings() {
}

void create_screen_pin_entry() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.pin_entry = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff68d284), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(obj, gesture_event_cb, LV_EVENT_GESTURE, NULL);    // ADD CALLBACK
    {
        lv_obj_t *parent_obj = obj;
        {
            // lbl_pin_start_title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_pin_start_title = obj;
            lv_obj_set_pos(obj, 112, 0);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Enter PIN");
        }
        {
            // btn_matrix_PIN
            lv_obj_t *obj = lv_buttonmatrix_create(parent_obj);
            objects.btn_matrix_pin = obj;
            lv_obj_set_pos(obj, 0, 69);
            lv_obj_set_size(obj, LV_PCT(100), 171);
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
            // lbl_show_PIN
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_show_pin = obj;
            lv_obj_set_pos(obj, 113, 30);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "123456");
        }
    }
    
    tick_screen_pin_entry();
}

void tick_screen_pin_entry() {
}

void create_screen_diagnostics() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.diagnostics = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 320, 240);
    add_style_page_black_text_white(obj);
    lv_obj_add_event_cb(obj, gesture_event_cb, LV_EVENT_GESTURE, NULL);    // ADD CALLBACK
    {
        lv_obj_t *parent_obj = obj;
        {
            // lbl_diagnostics_title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_diagnostics_title = obj;
            lv_obj_set_pos(obj, 100, 0);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Diagnostics");
        }
    }
    
    tick_screen_diagnostics();
}

void tick_screen_diagnostics() {
}



typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_home,
    tick_screen_weather,
    tick_screen_meshtastic,
    tick_screen_maintenance,
    tick_screen_settings,
    tick_screen_pin_entry,
    tick_screen_diagnostics,
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
    create_screen_meshtastic();
    create_screen_maintenance();
    create_screen_settings();
    create_screen_pin_entry();
    create_screen_diagnostics();
}
