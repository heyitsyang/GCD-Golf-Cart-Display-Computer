/*
 * prototypes.h
 */

void my_print(lv_log_level_t level, const char *buf);
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data);
void mockBarValue();
static void btn_reset_value_handler(lv_event_t *e);
static void btn_random_value_handler(lv_event_t *e);

