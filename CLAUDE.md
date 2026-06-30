# zmk-config-toucan

ZMK firmware config for the Toucan — a wireless split keyboard with a Cirque Pinnacle trackpad on the right half. The left half acts as central (BLE) or can be replaced by the XIAO BLE dongle as central.

Builds 5 firmware images via GitHub Actions: `toucan_right`, `toucan_left_central`, `toucan_left_peripheral`, `toucan_dongle`, `settings_reset`.

## Architecture

All trackpad intelligence runs on the **right half (peripheral)**:

1. `periph_gesture` (`zmk,input-peripheral-gesture`) processes raw Cirque ABS events and:
   - Converts to REL deltas (abs-to-rel with max-delta clamping)
   - Tracks a 5-event velocity window
   - On touch start: cancels inertial animation, presses virtual touch key
   - On touch end: starts inertial animation if velocity above threshold, releases touch key
   - Inertial animation: 16ms timer, decays velocity, injects REL events via `input_report_rel()`
   - Only non-zero REL pairs cross BLE — zero-delta frames are dropped entirely

2. `touch_kscan` (`zmk,kscan-touch-detect`) is a virtual kscan key at RC(3,9) wired into `kscan_composite`. Touch start/end from `periph_gesture` drives it. The key carries any ZMK behavior in the keymap.

The **central (left half or dongle)** receives only REL events and virtual key events. It:
- Routes REL through `zip_xy_scaler 75 100` → cursor
- On NAV layer (4): routes REL through `zip_xy_to_scroll_mapper` + `zip_scroll_transform X_INVERT` → scroll
- Touch key in BASE layer = `&mo 1` (activates PAD layer for mouse buttons while touching)

## Key files

- `boards/shields/toucan/toucan_right.overlay` — Cirque hardware, `periph_gesture`, `touch_kscan`, `kscan_composite`
- `boards/shields/toucan/toucan_left_central.overlay` — central input chain (scaler + scroller)
- `boards/shields/toucan_dongle/toucan_dongle.overlay` — same central chain for dongle
- `boards/shields/toucan/toucan_layout.dtsi` — 43-key layout including virtual touch key at RC(3,9)
- `config/toucan.keymap` — 43 bindings per layer; touch key = `&mo 1` in BASE, `&trans` elsewhere
- `config/west.yml` — pulls in `zmk-input-gestures` from `mmzim05` remote

## Current tuning (`periph_gesture` on right half)

| Property | Value | Meaning |
|---|---|---|
| `max-delta` | 60 | Clamps per-poll jump glitches |
| `touch-timeout-ms` | 30 | Ms silence → touch end |
| `velocity-threshold` | 3 | 0.3 raw_px/ms — filters stationary lifts |
| `decay-percent` | 9 | 9% speed lost per 16ms inertial frame |
| `speed-scale` | 100 | Matches `zip_xy_scaler 75 100` numerator on central |

Scaler on central: `zip_xy_scaler 75 100` (applied to both cursor and inertial REL events).

## Workflow

**Always commit and push after every change.** GitHub Actions builds on push.

## Picking up in a new session

1. Read this file and `CLAUDE.md` in `zmk-input-gestures`
2. Gesture/tuning changes: edit `periph_gesture` node in `toucan_right.overlay`, commit + push
3. Scaler/scroll changes: edit `toucan_left_central.overlay` and `toucan_dongle.overlay`
4. Trigger build: `gh workflow run build.yml --ref main`
5. Watch build: `gh run watch <run-id> --exit-status`
6. Download: `gh run download <run-id> --repo mmzim05/zmk-config-toucan --dir ~/Downloads/firmware`
7. Flash: right half always needs reflash for tuning changes; left central or dongle for central chain changes
