/*
 * SPDX-License-Identifier: MIT
 *
 * Status sync: central (dongle) -> peripheral (left half display).
 *
 * In a split, layer state, host connection and the other halves' batteries
 * only exist on the central. The dongle pushes them to each peripheral by
 * invoking the `status_sync` behavior there over the split link, with the
 * state packed into param1 below. Keep the encoding in this one header so
 * both firmwares agree on it.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Device name of the behavior node on the peripheral (max 15 chars). */
#define STATUS_SYNC_BEHAVIOR_NAME "status_sync"

struct status_sync_payload {
    uint8_t layer_id;      /* highest active layer, keymap (devicetree) order */
    uint8_t peer_battery;  /* other half's battery %, 0 = unknown */
    uint8_t profile_index; /* active BLE profile */
    bool usb;              /* selected host endpoint is USB (else BLE) */
    bool profile_connected;
    bool profile_bonded;
};

/* param1: [7:0] layer_id, [15:8] peer_battery, [19:16] profile_index,
 *         [20] usb, [21] profile_connected, [22] profile_bonded */
static inline uint32_t status_sync_encode(const struct status_sync_payload *p) {
    return (uint32_t)p->layer_id | ((uint32_t)p->peer_battery << 8) |
           ((uint32_t)(p->profile_index & 0xF) << 16) | ((uint32_t)p->usb << 20) |
           ((uint32_t)p->profile_connected << 21) | ((uint32_t)p->profile_bonded << 22);
}

static inline struct status_sync_payload status_sync_decode(uint32_t v) {
    return (struct status_sync_payload){
        .layer_id = v & 0xFF,
        .peer_battery = (v >> 8) & 0xFF,
        .profile_index = (v >> 16) & 0xF,
        .usb = (v >> 20) & 1,
        .profile_connected = (v >> 21) & 1,
        .profile_bonded = (v >> 22) & 1,
    };
}
