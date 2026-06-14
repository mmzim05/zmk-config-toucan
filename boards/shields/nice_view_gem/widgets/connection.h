#pragma once

#include <lvgl.h>
#include "util.h"

struct connection_status_state {
    bool connected;
};

void draw_connection_status(lv_obj_t *canvas, const struct status_state *state);
