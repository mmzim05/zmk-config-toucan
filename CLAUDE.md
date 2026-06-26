# zmk-config-toucan

ZMK firmware config for the Toucan — a wireless split keyboard with a Cirque Pinnacle trackpad on the right half and a XIAO BLE dongle as the central. Builds firmware for left half, right half, and dongle via GitHub Actions.

## What was done

- Switched Cirque to absolute mode (`absolute-mode`, `x-invert`, `y-invert` in `toucan_right.overlay`)
- Added `zmk-input-gestures` as a west module (points to `mmzim05/zmk-input-gestures` main branch)
- Dongle overlay wires up the full input processor chain:
  `zip_gestures → zip_abs_to_rel_cursor → zip_xy_scaler 75 100`
- Tuned inertial cursor: 5-event velocity window, speed-scale=75 to match scaler, threshold=3 (0.3 raw_px/ms), decay=9
- Tuned inertial scroll: same 5-event velocity window, threshold=3 (0.3 raw_px/ms), decay=7

## Key files

- `boards/shields/toucan_dongle/toucan_dongle.overlay` — input processor chain and all gesture tuning
- `boards/shields/toucan/toucan_right.overlay` — Cirque hardware config (absolute mode, axis inversion)
- `config/west.yml` — pulls in `zmk-input-gestures` from `mmzim05` remote

## Current tuning

`&zip_gestures` (cursor):

| Property | Value | Meaning |
|---|---|---|
| `inertial-cursor-velocity-threshold` | 3 | 0.3 raw_px/ms — filters stationary lifts only |
| `inertial-cursor-decay-percent` | 9 | 9% speed lost per 16ms frame |
| `inertial-cursor-speed-scale` | 75 | matches `zip_xy_scaler 75 100` |

`zip_scroll_gestures` (scroll):

| Property | Value | Meaning |
|---|---|---|
| `inertial-scroll-velocity-threshold` | 3 | 0.3 raw_px/ms — filters stationary lifts only |
| `inertial-scroll-decay-percent` | 7 | 7% speed lost per 16ms frame |

## Workflow

**Always commit and push after every change.** GitHub Actions builds on push — no push = no new firmware.

## Picking up in a new session

1. Read this file and `CLAUDE.md` in `zmk-input-gestures`
2. Gesture/tuning changes: edit `toucan_dongle.overlay`, commit + push to `main`
3. Cirque hardware changes: edit `toucan_right.overlay` (requires reflashing right half too)
4. Trigger build: `gh workflow run build.yml --ref main`
5. Watch build: `gh run watch <run-id> --exit-status`
6. Download firmware: `gh run download <run-id> --repo mmzim05/zmk-config-toucan --dir ~/Downloads/firmware`
7. Flash only the dongle unless right half overlay changed
