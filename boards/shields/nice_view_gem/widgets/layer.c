#include <zephyr/kernel.h>
#include <drivers/behavior.h>
#include <stdio.h>
#include <string.h>

#include "layer.h"
#include "../assets/custom_fonts.h"
#include <zmk/physical_layouts.h>
#include <zmk/keymap.h>
#include <zmk/matrix.h>

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

static const char *layer_name(uint8_t layer_id) { return zmk_keymap_layer_name(layer_id); }

#elif DT_HAS_COMPAT_STATUS_OKAY(zmk_keymap)

// The keymap runtime only exists on the central; a peripheral reads the names
// straight from the keymap devicetree, where layer IDs are child order.
#define LAYER_NAME(node) DT_PROP_OR(node, display_name, ""),
static const char *const layer_names[] = {
    DT_FOREACH_CHILD(DT_INST(0, zmk_keymap), LAYER_NAME)};

static const char *layer_name(uint8_t layer_id) {
    return layer_id < ARRAY_SIZE(layer_names) ? layer_names[layer_id] : NULL;
}

#else

static const char *layer_name(uint8_t layer_id) { return NULL; }

#endif

void draw_layer_status(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_24, LV_TEXT_ALIGN_CENTER);

    char fallback_layer_name[16];

    const char *name = layer_name(state->layer_id);

    if (name == NULL || name[0] == '\0') {
        sprintf(fallback_layer_name, "L#%" PRIu8, state->layer_id);

        name = fallback_layer_name;
    }

    lv_canvas_draw_text(canvas, 0, 70, SCREEN_WIDTH, &label_dsc, name);
}
