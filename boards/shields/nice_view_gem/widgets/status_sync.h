#pragma once

#include <zmk/event_manager.h>
#include <toucan/status_sync.h>

/* Raised on the peripheral each time the central pushes its status. */
struct zmk_status_sync_changed {
    struct status_sync_payload payload;
};

ZMK_EVENT_DECLARE(zmk_status_sync_changed);
