/*
 * Central side of the battery relay. Peripherals can't see each other, so the
 * dongle forwards each half's battery level to every other half by invoking
 * the "bat_rly" behavior on it (see nice_view_gem/widgets/battery_relay.c).
 * Halves without that behavior just log a failed invoke and ignore it.
 *
 * Sends are debounced: a peripheral's battery is read right after it connects,
 * so the sync also covers "left half reconnected after the right half last
 * reported". The delay lets split service discovery finish first.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/split/central.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define BATTERY_RELAY_BEHAVIOR "bat_rly"
#define BATTERY_RELAY_SYNC_DELAY K_SECONDS(3)

static uint8_t levels[ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT];

static void battery_relay_sync(struct k_work *work) {
    for (uint8_t to = 0; to < ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT; to++) {
        for (uint8_t from = 0; from < ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT; from++) {
            if (from == to || levels[from] == 0) {
                continue;
            }
            struct zmk_behavior_binding binding = {
                .behavior_dev = BATTERY_RELAY_BEHAVIOR,
                .param1 = levels[from],
            };
            struct zmk_behavior_binding_event event = {
                .position = 0,
                .timestamp = k_uptime_get(),
            };
            int err = zmk_split_central_invoke_behavior(to, &binding, event, true);
            if (err) {
                LOG_DBG("Battery relay %d -> %d failed: %d", from, to, err);
            }
        }
    }
}

static K_WORK_DELAYABLE_DEFINE(battery_relay_sync_work, battery_relay_sync);

static int battery_relay_listener(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev =
        as_zmk_peripheral_battery_state_changed(eh);
    if (ev == NULL || ev->source >= ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    levels[ev->source] = ev->state_of_charge;
    k_work_reschedule(&battery_relay_sync_work, BATTERY_RELAY_SYNC_DELAY);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(battery_relay, battery_relay_listener);
ZMK_SUBSCRIPTION(battery_relay, zmk_peripheral_battery_state_changed);
