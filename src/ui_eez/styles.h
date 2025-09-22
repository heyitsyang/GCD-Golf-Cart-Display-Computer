#ifndef EEZ_LVGL_UI_STYLES_H
#define EEZ_LVGL_UI_STYLES_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Style: Page black text white
lv_style_t *get_style_page_black_text_white_MAIN_DEFAULT();
void add_style_page_black_text_white(lv_obj_t *obj);
void remove_style_page_black_text_white(lv_obj_t *obj);

// Style: Cart Normal Page
lv_style_t *get_style_cart_normal_page_MAIN_DEFAULT();
void add_style_cart_normal_page(lv_obj_t *obj);
void remove_style_cart_normal_page(lv_obj_t *obj);



#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_STYLES_H*/