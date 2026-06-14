#include <zephyr/kernel.h>
#include "connection.h"
#include "../assets/custom_fonts.h"

static void draw_dongle_connected(lv_obj_t *canvas) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_LEFT);
    lv_canvas_draw_text(canvas, 12, 140, SCREEN_WIDTH-8, &label_dsc, "DONGLE");
}

static void draw_dongle_disconnected(lv_obj_t *canvas) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_LEFT);
    lv_canvas_draw_text(canvas, 12, 140, SCREEN_WIDTH-8, &label_dsc, "NO LINK");
}

void draw_connection_status(lv_obj_t *canvas, const struct status_state *state) {
    if (state->connected) {
        draw_dongle_connected(canvas);
    } else {
        draw_dongle_disconnected(canvas);
    }
}
