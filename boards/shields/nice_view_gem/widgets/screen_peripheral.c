#include "screen.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include "../assets/custom_fonts.h"

#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/usb.h>

#include "battery.h"
#include "battery_peripheral.h"
#include "connection.h"
#include "layer.h"
#include "output.h"
#include "profile.h"
#include "sleep.h"
#include "status_sync.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/**
 * Draw buffer
 **/

static void draw_top(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);
    fill_background(canvas);

    if (is_sleep_screen_active()) {
        draw_sleep_screen(canvas);
        return;
    }

    draw_battery_status(canvas, state);

    if (state->connected && state->synced) {
        draw_output_status(canvas, state);
        draw_layer_status(canvas, state);
        draw_profile_status(canvas, state);
        draw_battery_peripheral_status(canvas, state);
    } else {
        draw_connection_status(canvas, state);
    }
}

/**
 * Battery status (this half's own battery)
 **/

static void set_battery_status(struct zmk_widget_screen *widget,
                               struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    widget->state.battery = state.level;

    draw_top(widget->obj, widget->cbuf, &widget->state);
}

static void battery_status_update_cb(struct battery_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_status(widget, state); }
}

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);

    return (struct battery_status_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state);

ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

/**
 * Connection status (link to the dongle)
 **/

static void set_connection_status(struct zmk_widget_screen *widget,
                                  struct connection_status_state state) {
    widget->state.connected = state.connected;
    if (!state.connected) {
        // Drop the central's status until it pushes a fresh one
        widget->state.synced = false;
    }

    draw_top(widget->obj, widget->cbuf, &widget->state);
}

static void connection_status_update_cb(struct connection_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_connection_status(widget, state); }
}

static struct connection_status_state connection_status_get_state(const zmk_event_t *eh) {
    const struct zmk_split_peripheral_status_changed *ev =
        as_zmk_split_peripheral_status_changed(eh);

    return (struct connection_status_state){
        .connected = (ev != NULL) ? ev->connected : false,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_connection_status, struct connection_status_state,
                            connection_status_update_cb, connection_status_get_state);

ZMK_SUBSCRIPTION(widget_connection_status, zmk_split_peripheral_status_changed);

/**
 * Status pushed from the central (layer, host connection, other half's battery)
 **/

struct status_sync_state {
    bool valid;
    struct status_sync_payload payload;
};

static void set_status_sync(struct zmk_widget_screen *widget, struct status_sync_state state) {
    if (!state.valid) {
        return;
    }

    const struct status_sync_payload *p = &state.payload;
    widget->state.synced = true;
    widget->state.layer_id = p->layer_id;
    widget->state.battery_p = p->peer_battery;
    widget->state.selected_endpoint = (struct zmk_endpoint_instance){
        .transport = p->usb ? ZMK_TRANSPORT_USB : ZMK_TRANSPORT_BLE,
        .ble = {.profile_index = p->profile_index},
    };
    widget->state.active_profile_index = p->profile_index;
    widget->state.active_profile_connected = p->profile_connected;
    widget->state.active_profile_bonded = p->profile_bonded;

    draw_top(widget->obj, widget->cbuf, &widget->state);
}

static void status_sync_update_cb(struct status_sync_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_status_sync(widget, state); }
}

static struct status_sync_state status_sync_get_state(const zmk_event_t *eh) {
    const struct zmk_status_sync_changed *ev = as_zmk_status_sync_changed(eh);

    return (struct status_sync_state){
        .valid = ev != NULL,
        .payload = (ev != NULL) ? ev->payload : (struct status_sync_payload){0},
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_status_sync, struct status_sync_state, status_sync_update_cb,
                            status_sync_get_state);

ZMK_SUBSCRIPTION(widget_status_sync, zmk_status_sync_changed);

/**
 * Activity state handling for sleep screen
 **/

static void force_redraw_all_widgets(void) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        draw_top(widget->obj, widget->cbuf, &widget->state);
    }
}

static int display_activity_event_handler(const zmk_event_t *eh) {
    struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev == NULL) {
        return -ENOTSUP;
    }

    switch (ev->state) {
    case ZMK_ACTIVITY_ACTIVE:
        set_sleep_screen_active(false);
        break;
    case ZMK_ACTIVITY_SLEEP:
        set_sleep_screen_active(true);
        force_redraw_all_widgets();
        // Force LVGL to process pending updates and flush to display hardware
        // before the CPU enters deep sleep
        lv_task_handler();
        lv_refr_now(NULL);
        break;
    default:
        break; // ignore other states (like IDLE)
    }
    return 0;
}

ZMK_LISTENER(nice_view_gem_peripheral_display, display_activity_event_handler);
ZMK_SUBSCRIPTION(nice_view_gem_peripheral_display, zmk_activity_state_changed);

/**
 * Initialization
 **/

int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, SCREEN_WIDTH, SCREEN_HEIGHT);

    lv_obj_t *top = lv_canvas_create(widget->obj);
    lv_obj_align(top, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_canvas_set_buffer(top, widget->cbuf, SCREEN_WIDTH, SCREEN_HEIGHT, LV_IMG_CF_TRUE_COLOR);

    sys_slist_append(&widgets, &widget->node);
    widget_battery_status_init();
    widget_connection_status_init();
    widget_status_sync_init();

    return 0;
}

lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget) { return widget->obj; }
