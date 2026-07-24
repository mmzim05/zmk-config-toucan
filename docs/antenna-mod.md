# External antenna mod (seeeduino_xiao_ble)

Why: RSSI logging (see `CONFIG_ZMK_RSSI_LOGGER` in zmk-input-gestures) showed
-70 to -82 dBm even at close range on the dongle's split links, well below
what a healthy link should read. A real disconnect (HCI reason 0x08,
Connection Timeout) was captured at -82 dBm at ~3-4m, and that time the
right half needed a hard reset rather than auto-reconnecting. Root cause:
the stock XIAO BLE (nRF52840) chip antenna has a weak link budget. This mod
replaces it with a real external antenna, properly matched to 50 ohm.

Start with the **right half** (the one that hard-crashed). Re-run the RSSI
diagnostic after to confirm improvement before deciding whether left/dongle
need the same treatment.

## Parts

- Antenna: **Adafruit 2308** — 2.4GHz Mini Flexible Antenna, u.FL, 100mm,
  4dBi, 50Ω (adafruit.com/product/2308)
- Nothing else to buy — cut the antenna's own u.FL pigtail off and solder
  the bare coax directly to the board. No separate pigtail/connector needed.

## Tools

- Fine-tip soldering iron (or hot air, preferred for removing the tiny
  chip antenna without lifting the pad)
- Flux
- Fine tweezers
- Wire strippers or a sharp hobby knife (for the 1.13mm coax)
- Multimeter (optional, to confirm no shorts after)

## Board-side circuit (from schematic)

Antenna-matching pi-network feeding `ANT1` (Rainsun AN3216-245):

```
chip RF trace → [node A] → C38 (4.7nH series) → [node B] → ANT1 pin 1 (feed)
                   |                                |
                  C36 (0.8pF to GND)            C37 (0.5pF to GND)

ANT1 pin 2 = GND direct
ANT1 pin 3 = GND via C10 (1nF)
```

This pi-network (`C36`/`C37`/`C38`) is tuned for the AN3216 chip antenna's
specific impedance — it is NOT correct for a 50Ω external antenna and must
come out. The chip's own radio-side matching network (separate, closer to
the SoC, not shown above) stays untouched — per Nordic's guidance, a 50Ω
external antenna feeds in *after* that stage and does not need the
antenna-specific pi-network at all.

## Steps

1. **Remove `ANT1`, `C36`, `C37`, `C38`** from the board. Hot air is safer
   than an iron here — these are tiny 3216/0402-scale parts and it's easy
   to lift a pad with direct iron heat.
2. **Prep the antenna's coax**: cut off the u.FL connector, strip back
   ~3-4mm of outer jacket, fold back the exposed braid (this is your
   ground lead), strip ~1.5-2mm of the inner dielectric to expose the
   center conductor. Lightly tin both the center conductor and the
   folded-back braid before soldering to the board — the strands fray
   easily otherwise.
3. **Solder center conductor → node A** — the pad where `C38` met the
   incoming trace from the chip (the true 50Ω point, now that the
   antenna-specific network is gone).
4. **Solder braid/shield → nearby GND** — the old `C36` ground pad or
   `ANT1` pin 2/3 pad both work.
5. Route the antenna element out of the case, mount it wherever it fits
   (avoid running it directly against a ground plane or metal enclosure
   wall if possible).
6. Optional: multimeter continuity check center-to-ground should read
   open (no short) before powering on.

## Verify

1. Reflash `toucan_dongle_rssi_debug` (or the equivalent debug build for
   whichever half you modded) — see the existing GitHub Actions artifact,
   or trigger a fresh build if the branch has moved on.
2. Watch `/dev/ttyACM0` (or wherever the dongle enumerates) for the
   `rssi_logger` output at the same test distance/spot that used to crash.
3. Compare dBm before/after. Expect meaningfully better than the old
   -70 to -82 dBm baseline.
4. Reflash the normal (non-debug) build once satisfied — the debug variant
   was only for testing and shouldn't stay on permanently (extra RAM/USB
   logging overhead).
