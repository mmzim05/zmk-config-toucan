/*
 * Peripheral side of the battery relay. Split peripherals only talk to the
 * central, so this half can't read the other half's battery directly. The
 * dongle (toucan_dongle/battery_relay.c) invokes this behavior over split
 * with param1 = the other half's battery level; we re-raise it locally as a
 * zmk_peripheral_battery_state_changed so the display can draw it.
 */

#define DT_DRV_COMPAT zmk_behavior_battery_relay

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int on_battery_relay_pressed(struct zmk_behavior_binding *binding,
                                    struct zmk_behavior_binding_event event) {
    raise_zmk_peripheral_battery_state_changed((struct zmk_peripheral_battery_state_changed){
        .source = 0,
        .state_of_charge = (uint8_t)binding->param1,
    });
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_battery_relay_released(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_battery_relay_driver_api = {
    .binding_pressed = on_battery_relay_pressed,
    .binding_released = on_battery_relay_released,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_battery_relay_driver_api);

#endif
