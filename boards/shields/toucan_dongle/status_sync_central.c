/*
 * SPDX-License-Identifier: MIT
 *
 * Dongle side of status sync (see include/toucan/status_sync.h): sends layer,
 * host connection and the other half's battery to each peripheral whenever
 * one changes, plus a periodic refresh so a half that (re)connects catches up.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/split/central.h>

#include <toucan/status_sync.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define STATUS_SYNC_DEBOUNCE K_MSEC(20)
#define STATUS_SYNC_REFRESH K_SECONDS(5)

static void status_sync_send(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(status_sync_work, status_sync_send);

/* Battery of the first connected peripheral other than `self`. */
static uint8_t peer_battery(uint8_t self) {
    for (uint8_t i = 0; i < ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT; i++) {
        uint8_t level = 0;
        if (i != self && zmk_split_central_get_peripheral_battery_level(i, &level) == 0 &&
            level > 0) {
            return level;
        }
    }
    return 0;
}

static void status_sync_send(struct k_work *work) {
    struct status_sync_payload payload = {
        .layer_id = zmk_keymap_layer_index_to_id(zmk_keymap_highest_layer_active()),
        .profile_index = zmk_ble_active_profile_index(),
        .usb = zmk_endpoints_selected().transport == ZMK_TRANSPORT_USB,
        .profile_connected = zmk_ble_active_profile_is_connected(),
        .profile_bonded = !zmk_ble_active_profile_is_open(),
    };

    /* Disconnected sources are dropped by the split transport. */
    for (uint8_t i = 0; i < ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT; i++) {
        payload.peer_battery = peer_battery(i);
        struct zmk_behavior_binding binding = {
            .behavior_dev = STATUS_SYNC_BEHAVIOR_NAME,
            .param1 = status_sync_encode(&payload),
        };
        struct zmk_behavior_binding_event event = {.timestamp = k_uptime_get()};
        int err = zmk_split_central_invoke_behavior(i, &binding, event, true);
        if (err) {
            LOG_DBG("status sync to peripheral %d failed (%d)", i, err);
        }
    }

    k_work_reschedule(&status_sync_work, STATUS_SYNC_REFRESH);
}

static int status_sync_listener(const zmk_event_t *eh) {
    k_work_reschedule(&status_sync_work, STATUS_SYNC_DEBOUNCE);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(status_sync, status_sync_listener);
ZMK_SUBSCRIPTION(status_sync, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(status_sync, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(status_sync, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(status_sync, zmk_peripheral_battery_state_changed);
