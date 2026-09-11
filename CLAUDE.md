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
| `decay-percent` | 6 | 6% speed lost per 10ms inertial frame — retuned for the 10ms tick (was 17%@32ms) to keep the same real-time glide feel |
| `speed-scale` | 100 | Matches `zip_xy_scaler 75 100` numerator on central |
| `rotate-cdeg` | 3000 | 30° CCW rotation to compensate physical trackpad tilt |
| `touch-confirm-samples` | 2 | Consecutive ABS samples required before a touch is confirmed (kscan press, idle-timer reset). Filters single-sample phantom touches from electrical noise so they can't keep the right half awake |

Inertial tick: always 10ms (100 Hz), matching the live Cirque poll rate — no slower post-lift rate. A prior tuning (`decay-percent`@17, tick halved to 32ms) traded cursor smoothness after lift for lower BLE notification rate, as a workaround for right-half crashes at range. That workaround is reverted: the crashes are the XIAO's weak stock antenna (see `docs/antenna-mod.md`), not something to fix by slowing the cursor. If range instability recurs, look at the antenna mod / RSSI logging, not the inertial tick.

Scaler on central: `zip_xy_scaler 75 100` (applied to both cursor and inertial REL events).

## Stack configuration (right half)

| Thread | Stack | Set by |
|---|---|---|
| gesture_work_q | 4096B | `CONFIG_ZMK_INPUT_PERIPHERAL_GESTURE_WORKQ_STACK_SIZE=4096` |
| INPUT THREAD | 4096B | `CONFIG_INPUT_THREAD_STACK_SIZE=4096` |
| System workqueue | 1024B (default) | not overridden |

- gesture_work_q runs all three work handlers to keep the `kscan→split→BLE` chain off the system workqueue
- `CONFIG_BT_BUF_ACL_TX_COUNT=10` (default 3) — extra ACL TX buffers absorb retransmission backpressure at long range

## Sleep / battery (right half)

Two independent power domains, both matter:

- **MCU deep sleep**: `CONFIG_ZMK_IDLE_TIMEOUT=30000` (30s → idle state), then `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=300000` (additional 5 min → deep sleep / system off). Kept short (unlike the left central's long timeout) because the right half is a peripheral: waking it re-pairs to the central automatically in the background, there's no user-facing host re-bond to avoid. ZMK only resets this timer on `zmk_position_state_changed` (real or virtual key press) and `zmk_sensor_event` — trackpad REL motion itself does not reset it. The virtual touch key (`touch_kscan`) does fire `zmk_position_state_changed`, gated by `touch-confirm-samples` in `periph_gesture` (see `CLAUDE.md` in `zmk-input-gestures`) so electrical-noise phantom touches can't hold this timer open indefinitely.
- **Waking from deep sleep — do not enable `CONFIG_ZMK_PM_SOFT_OFF` here.** Tried it once (the old comment thought it was required for the Cirque suspend/resume notifications); it broke waking the right half from deep sleep entirely — only the physical reset button worked, not real key presses. Root cause: `CONFIG_ZMK_SLEEP=y` alone already `select`s `ZMK_PM_DEVICE_SUSPEND_RESUME` in ZMK's own Kconfig, so Soft Off was never needed for that. Soft Off is also unreachable in this config (no `&soft_off` keymap binding), and per ZMK docs it's far more restrictive about wake sources than regular deep sleep — it needs an explicit `zmk,soft-off-wakeup-sources` devicetree node to allow anything but the reset button to wake it, which was never defined here. Regular deep sleep wakes fine on its own via the `wakeup-source;` property already on `kscan0` (`toucan.dtsi`).
- **Cirque chip scan**: without chip-level power management the Pinnacle scans at its native ~100Hz continuously any time the MCU is awake, whether or not you're touching it — datasheet-rated ~2.9mA active vs ~40µA asleep, so this can dominate battery life on its own. `CONFIG_ZMK_INPUT_PINNACLE_IDLE_SLEEPER` + `sleep;` on the `glidepoint` node (`toucan_right.overlay`) would put the chip itself to sleep (`zmk_pinnacle_idle_sleeper.c` in `cirque-input-module` flips it on after `CONFIG_ZMK_IDLE_TIMEOUT`, 30s). **Tried and reverted**: once asleep, the chip's own Sleep Interval register (hardcoded to 255 in `input_pinnacle.c`'s init, not exposed via Kconfig/DT) governs how often it wakes to check for a touch, not a flat ~300ms — a touch landing between checks is missed, so in practice it took several seconds of moving a finger around before the cursor responded. Left disabled (commented out in both files) in favor of just the MCU-level deep sleep below, which has no responsiveness cost. Revisiting this would mean forking `cirque-input-module` to make the Sleep Interval short/configurable.

## Workflow

**Always commit and push after every change.** GitHub Actions builds on push.

## Picking up in a new session

1. Read this file and `CLAUDE.md` in `zmk-input-gestures`
2. Gesture/tuning changes: edit `periph_gesture` node in `toucan_right.overlay`, commit + push
3. Scaler/scroll changes: edit `toucan_left_central.overlay` and `toucan_dongle.overlay`
4. Watch build: `gh run watch <run-id> --repo mmzim05/zmk-config-toucan --exit-status`
5. Download: `gh run download <run-id> --repo mmzim05/zmk-config-toucan --dir ~/Downloads/firmware`
6. Flash: right half always needs reflash for tuning changes; left central or dongle for central chain changes
