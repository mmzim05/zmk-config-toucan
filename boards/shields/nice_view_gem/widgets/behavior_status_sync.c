/*
 * SPDX-License-Identifier: MIT
 *
 * Peripheral side of status sync: the central invokes this behavior over the
 * split link with its status packed into param1 (include/toucan/status_sync.h).
 */

#define DT_DRV_COMPAT zmk_behavior_status_sync

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>

#include "status_sync.h"

ZMK_EVENT_IMPL(zmk_status_sync_changed);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int on_status_sync_pressed(struct zmk_behavior_binding *binding,
                                  struct zmk_behavior_binding_event event) {
    raise_zmk_status_sync_changed(
        (struct zmk_status_sync_changed){.payload = status_sync_decode(binding->param1)});
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_status_sync_released(struct zmk_behavior_binding *binding,
                                   struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_status_sync_driver_api = {
    .binding_pressed = on_status_sync_pressed,
    .binding_released = on_status_sync_released,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_status_sync_driver_api);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
