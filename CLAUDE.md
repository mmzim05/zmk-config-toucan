# zmk-config-toucan

ZMK firmware config for the Toucan — a wireless split keyboard with a Cirque Pinnacle trackpad on the right half. The left half acts as central (BLE) or can be replaced by the XIAO BLE dongle as central.

Builds 5 firmware images via GitHub Actions: `toucan_right`, `toucan_left_central`, `toucan_left_peripheral`, `toucan_dongle`, `settings_reset`.

## Architecture

All trackpad intelligence runs on the **right half (peripheral)**:

1. `periph_gesture` (`zmk,input-peripheral-gesture`) processes raw Cirque ABS events and:
   - Converts to REL deltas (abs-to-rel with max-delta clamping)
   - Applies virtual rotation (rotate-cdeg, Q8 sin/cos precomputed at init)
   - Tracks a 5-event velocity window (Q8 fixed-point, no float in hot path)
   - On touch start: submits `touch_start_work` to `gesture_work_q` (NOT the INPUT THREAD — avoids deep BLE call chain on INPUT THREAD stack), cancels inertial
   - On touch end (20ms timeout): computes velocity, starts inertial if above threshold, releases touch key
   - Inertial animation: 32ms timer, decays velocity, injects REL events via `input_report_rel()`
   - Only non-zero REL pairs cross BLE — zero-delta frames dropped entirely

2. `touch_kscan` (`zmk,kscan-touch-detect`) is a virtual kscan key at RC(3,9) wired into `kscan_composite`. Touch start/end from `periph_gesture` drives it. The key carries any ZMK behavior in the keymap.

3. **gesture_work_q** — dedicated Zephyr work queue (4096B stack, priority 5) runs all three work items: `touch_start_work`, `touch_end_work`, `inertial_work`. Keeps the deep ZMK kscan→split→BLE call chain off both the INPUT THREAD and the system workqueue.

The **central (left half or dongle)** receives only REL events and virtual key events. It:
- Routes REL through `zip_xy_scaler 75 100` → cursor
- On scroll layer: routes REL through `zip_xy_to_scroll_mapper` + `zip_scroll_transform X_INVERT` → scroll
  - Left central: scroll on layer 4, no speed scaler
  - Dongle: scroll on layer 3, `zip_scroll_scaler 1 2` (half speed)
- Touch key in BASE layer = `&mo 1` (activates PAD layer for mouse buttons while touching)

## Key files

- `boards/shields/toucan/toucan_right.overlay` — Cirque hardware, `periph_gesture`, `touch_kscan`, `kscan_composite`
- `boards/shields/toucan/toucan_right.conf` — right half Kconfig (gesture workq stack, INPUT_THREAD stack, BLE buffers)
- `boards/shields/toucan/toucan_left_central.overlay` — central input chain (scaler + scroller, layer 4)
- `boards/shields/toucan/toucan_left_central.conf` — left central Kconfig (stack sizes, smooth scrolling, BLE CI)
- `boards/shields/toucan_dongle/toucan_dongle.overlay` — dongle input chain (scaler + scroller, layer 3, half scroll speed)
- `boards/shields/toucan/toucan_layout.dtsi` — 43-key layout including virtual touch key at RC(3,9)
- `config/toucan.keymap` — 43 bindings per layer; touch key = `&mo 1` in BASE, `&trans` elsewhere
- `config/west.yml` — pulls in `zmk-input-gestures` from `mmzim05` remote

## Current tuning (`periph_gesture` on right half)

| Property | Value | Meaning |
|---|---|---|
| `max-delta` | 60 | Clamps per-poll jump glitches |
| `touch-timeout-ms` | 20 | Ms silence → touch end (≥ 2× Cirque 10ms poll interval) |
| `velocity-threshold` | 3 | 0.3 raw_px/ms — filters stationary lifts |
| `decay-percent` | 17 | 17% speed lost per 32ms inertial frame (≈ 9% per 16ms) |
| `speed-scale` | 100 | Matches `zip_xy_scaler 75 100` numerator on central |
| `rotate-cdeg` | 3000 | 30° CCW rotation to compensate physical trackpad tilt |

Inertial tick: 32ms (31 Hz). Halved from 16ms to reduce BLE notification rate at long range.

Scaler on central: `zip_xy_scaler 75 100` (applied to both cursor and inertial REL events).

## Stack configuration (right half)

| Thread | Stack | Set by |
|---|---|---|
| gesture_work_q | 4096B | `CONFIG_ZMK_INPUT_PERIPHERAL_GESTURE_WORKQ_STACK_SIZE=4096` |
| INPUT THREAD | 4096B | `CONFIG_INPUT_THREAD_STACK_SIZE=4096` |
| System workqueue | 1024B (default) | not overridden |

- gesture_work_q runs all three work handlers to keep the `kscan→split→BLE` chain off the system workqueue
- `CONFIG_BT_BUF_ACL_TX_COUNT=10` (default 3) — extra ACL TX buffers absorb retransmission backpressure at long range

## Workflow

**Always commit and push after every change.** GitHub Actions builds on push.

## Picking up in a new session

1. Read this file and `CLAUDE.md` in `zmk-input-gestures`
2. Gesture/tuning changes: edit `periph_gesture` node in `toucan_right.overlay`, commit + push
3. Scaler/scroll changes: edit `toucan_left_central.overlay` and `toucan_dongle.overlay`
4. Watch build: `gh run watch <run-id> --repo mmzim05/zmk-config-toucan --exit-status`
5. Download: `gh run download <run-id> --repo mmzim05/zmk-config-toucan --dir ~/Downloads/firmware`
6. Flash: right half always needs reflash for tuning changes; left central or dongle for central chain changes
