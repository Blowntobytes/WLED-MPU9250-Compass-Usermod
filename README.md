# WLED MPU-9250 Compass Usermod

Turn any WS2812 / NeoPixel strip or ring into a **magnetic compass** using an
**MPU-9250 9-axis IMU** (gyroscope + accelerometer + magnetometer) on the I2C
bus. Designed for **WLED 16.0.0**.

The LEDs pointing towards **magnetic North** light up in one configurable
color/effect, every other LED lights up in a different configurable
color/effect, so you can see at a glance which way is North.

```
   ┌───────────────────────────────┐
   │        compass ring           │
   │    .  .  .  N  .  .  .        │   N  = north-sector LEDs (northColor)
   │    .                 .        │   .  = remaining LEDs   (otherColor)
   │    .    MPU-9250     .        │
   │    .   (I2C)         .        │
   │    .                 .        │
   │    .  .  .  .  .  .  .        │
   └───────────────────────────────┘
```

## Features

* **Tilt-compensated heading** computed from the magnetometer and
  accelerometer (pitch / roll / yaw), so the compass works even when the
  device is not perfectly level.
* **North sector display**: a configurable number of LEDs around the heading
  render in `northColor` + `northEffect`, the rest render in `otherColor` +
  `otherEffect`.
* **Two sensor options** (auto-detected or selected in settings):
  * **MPU-9250** 9-axis IMU — tilt-compensated heading, works even when not level.
  * **GY-271** magnetometer breakout (HMC5883L or QMC5883L) — simple 2-axis heading, perfect for a flat-mounted compass.
* **Full magnetometer calibration** built in: hard-iron offsets (bias) and
  soft-iron scaling (per-axis gain), either auto-computed with the on-device
  calibration routine or entered manually.
* **Per-LED effect emulation**: solid, blink, breath, rainbow, rainbow cycle
  and twinkle on either group of LEDs (see [Effects](#effects)).
* **"Falling Sand" effect**: a new WLED effect (falling-sand simulation)
  whose gravity direction follows the sensor's tilt — works on both 1D strips
  and 2D matrices, and needs only the accelerometer (so it works on a plain
  MPU-6500 with no magnetometer).
* **"Bubble Level" effect**: a tilt-driven spirit level — a configurable-size
  bubble floats opposite gravity, coloured from the Fx/Bg colour slots.
* **Sensor-driven "PS 1D Balance"**: the built-in WLED particle effect's
  virtual gravity is replaced with real tilt data from the accelerometer (it
  falls back to the original virtual force when this usermod isn't built in).
* **JSON API** integration: live heading (plus pitch/roll for the MPU-9250)
  and raw sensor data in `/json/state`, calibration commands, and sensor
  readout on the Info page.
* **No external libraries**: minimal self-contained drivers for the MPU-9250
  and GY-271 are bundled; they only use the Arduino `Wire` core. No conflicts
  with WLED's bundled libraries.
* **Graceful failure**: if no sensor is detected, `begin()` fails gracefully,
  the usermod stays disabled and your strip behaves normally.

## How it works

* **MPU-9250 mode**: the MPU-9250 embeds an MPU-6500 (accel + gyro) and an
  AK8963 magnetometer. Gravity is estimated from the accelerometer and used
  to derive roll/pitch; the raw magnetic field is then projected onto the
  horizontal plane (**tilt compensation**), so the heading stays correct even
  when the device isn't level.
* **GY-271 mode**: a bare magnetometer (HMC5883L at `0x1E` or QMC5883L at
  `0x0D`) gives the classic 2-axis heading `atan2(my, mx)`. There is no
  accelerometer, so the module must be held (or mounted) **level** for an
  accurate heading.
* The heading (0–360°) is mapped onto the ring: LED `n` covers the angular
  range `n * 360 / totalLeds` … `(n+1) * 360 / totalLeds` degrees, and the
  `northSize` LEDs centred on the heading are the "North sector".
* The heading is low-pass filtered (EMA in sine/cosine space) to remove
  sensor noise without jump artefacts at the 360°/0° boundary.
* The compass is re-read and re-rendered at ~20 Hz.

### Axis convention

Heading `0°` corresponds to the sensor's **+X axis** pointing towards
**magnetic North** while the device is level, increasing **clockwise** when
viewed from above (+Z up). If your sensor is mounted differently (e.g. the
forward axis is +Y, or the ring's 0° LED is elsewhere), fix it with the
`headingOffset` setting — see [Examples](#examples).

### Falling Sand effect

The usermod registers a custom effect called **"Falling Sand"** in the
WLED effect list. It is modelled on WLED's built-in **PS Hourglass** but
driven by the sensor's tilt:

* **1D strips**: the lit sand pile **sheds one pixel at a time** from the side
  opposite gravity (grains fall across the strip) and the low side
  **accumulates one pixel at a time**. Piles stay contiguous (no gaps).
  **Effect speed** sets the base drip pause (default ~50% faster), and
  **Flow** (Custom 1) shortens it so several grains are in flight at once —
  the more you raise Flow, the more the sand looks like it's pouring. Level
  the device to stop the flow; reverse the tilt to drain the sand back.
* **2D matrices**: a falling-sand grid; when it fills up it flips and falls
  the other way instead of disappearing.

Only the **accelerometer** is needed — it works with a genuine MPU-9250 *and*
with a plain MPU-6500 that has no magnetometer. Without any sensor the sand
simply falls straight down.

Select it as the effect for your segment, then use its sliders:

| Slider | Effect |
|--------|--------|
| Effect speed   | Base pause between grains (1D) / pour rate (2D). Higher = faster |
| Effect intensity | Sand amount |
| Custom 1 (Flow) | Shortens the drip pause so more grains are in flight at once (1D) / simulation steps (2D) |

The `tiltAxis` usermod setting selects which tilt direction drives the sand:

| `tiltAxis` | Behaviour |
|------------|-----------|
| `0` (default) | Both axes |
| `1` | **Left/right only** (roll) — forward/back tilt is ignored |
| `2` | **Forward/back only** (pitch) — left/right tilt is ignored |

The sand is coloured from the segment's **palette** (a gradient along the
strip/matrix), so any of WLED's built-in or custom palettes can be used. The
**secondary color** is the background. A directional dead-zone keeps the sand
steady (no flicker) while the device is level.

> If you have a magnetometer and the compass overlay is also enabled
> (`overlayEnabled`), the compass will paint over the sand — set
> `overlayEnabled = false` to show only the Falling Sand effect.

### Bubble Level effect

The usermod also registers a **"Bubble Level"** effect — a spirit level on the
strip. A **bubble** (configurable size) floats **opposite gravity** and sits
centred when the device is level. Tilt the strip and the bubble slides to the
raised end.

Select it as the effect for your segment and use its sliders:

| Slider | Effect |
|--------|--------|
| Effect speed   | Responsiveness — how fast the bubble tracks the tilt |
| Effect intensity | Bubble size (number of LEDs) |

Colours come from the usual colour picker screen: the **Fx** slot colours the
bubble and the **Bg** slot colours the rest of the strip. On a 2D matrix the
bubble is a vertical bar.

The bubble uses **continuous, un-normalised tilt** (no dead zone), so it
responds proportionally to even the smallest tilt — the closer to level, the
closer to centre — for maximum accuracy. (Resolution is limited by the number
of LEDs: on a 16-LED strip each LED is roughly 6% of the travel.)

## Hardware & wiring

| MPU-9250 (GY-9250 / GY-91) | GY-271 (HMC5883L / QMC5883L) | ESP32   | ESP8266  |
|----------------------------|------------------------------|---------|----------|
| VCC                        | VCC                          | 3.3V    | 3.3V     |
| GND                        | GND                          | GND     | GND      |
| SDA                        | SDA                          | GPIO 21 | GPIO 4   |
| SCL                        | SCL                          | GPIO 22 | GPIO 5   |
| AD0 (address select)       | —                            | GND -> 0x68, 3.3V -> 0x69 | |

* Supply voltage 3–5 V (the module's on-board regulators handle 3.3 V logic;
  do **not** feed 5 V into SDA/SCL).
* Pull-up resistors to 3.3 V on SDA/SCL are required (4.7 kΩ typical); most
  breakout boards already include them.
* SDA/SCL pins are configurable in the usermod settings. For the MPU-9250 the
  I2C address (`0x68` / `0x69`) is configurable too; the GY-271 is
  auto-detected at `0x1E` (HMC5883L) or `0x0D` (QMC5883L).
* The ring's first `totalLeds` LEDs (starting at LED 0) form the compass.

## Integration with WLED 16.0.0

The usermod is **compile-time optional**: it is only built when it appears in
the `custom_usermods` list of your PlatformIO environment, so the stock WLED
build is completely unaffected.

> This patch additionally makes two small core edits (so the built-in
> **PS 1D Balance** effect can read tilt from this usermod):
> `wled00/FX.cpp` (sensor force in `mode_particleBalance`, with a fallback to
> the original virtual force when the usermod isn't present) and
> `wled00/const.h` (a new usermod ID). Both are guarded so the stock build
> still works without the usermod.

### Option A — apply the patch (recommended)

From a pristine WLED **v16.0.0** checkout:

```bash
git apply integration/wled-16.0.0-integration.patch
```

This adds `usermods/mpu9250_compass/` to the tree.

### Option B — copy the folder

Copy `usermods/mpu9250_compass/` into your WLED checkout root:

```bash
cp -r usermods/mpu9250_compass <your-WLED>/usermods/
```

### Enable in your build

Create (or extend) `platformio_override.ini` in the WLED root with the
environment you want and add the usermod:

```ini
[env:compass_esp32]
extends = env:esp32dev
custom_usermods = mpu9250_compass
```

> Tip: if you build the stock `usermods` environment (`custom_usermods = *`),
> the folder is picked up automatically.

Then build:

```bash
pio run -e compass_esp32
```

The firmware will include the compass usermod; a stock `pio run -e esp32dev`
build stays byte-for-byte independent of it.

## Settings

All settings live under the `MPU9250Compass` object in `cfg.json` and are
editable on **Config → Usermod** in the WLED web UI.

| Key                | Type    | Default        | Description |
|--------------------|---------|----------------|-------------|
| `sensorEnabled`    | bool    | `true`         | Master switch for the usermod. |
| `overlayEnabled`   | bool    | `true`         | Compass LED overlay on/off (needs a magnetometer). Turn off to let the Hourglass (tilt) effect show unobstructed. |
| `sdaPin`           | int     | `21` (ESP32)   | I2C SDA pin. |
| `sclPin`           | int     | `22` (ESP32)   | I2C SCL pin. |
| `i2cAddress`       | int     | `104` (0x68)   | MPU-9250 I2C address; `104` = 0x68, `105` = 0x69. |
| `sensorType`       | int     | `0`            | `0` = auto-detect, `1` = MPU-9250 only, `2` = GY-271 (HMC5883L/QMC5883L) only. |
| `tiltAxis`         | int     | `0`            | Falling Sand tilt axis: `0` = both, `1` = left/right only, `2` = forward/back only. |
| `totalLeds`        | int     | `60`           | Number of LEDs that form the compass ring (LEDs 0…totalLeds-1). |
| `northColor`       | string  | `"FF0000"`     | Color (hex `RRGGBB`) of the North-sector LEDs. |
| `northEffect`      | int     | `0`            | Effect index for the North-sector LEDs (0 = solid), see [Effects](#effects). |
| `northSize`        | int     | `3`            | Number of LEDs in the North sector (1 LED ≈ 360/totalLeds degrees). |
| `otherColor`       | string  | `"0022FF"`     | Color (hex `RRGGBB`) of the remaining LEDs. |
| `otherEffect`      | int     | `0`            | Effect index for the remaining LEDs (0 = solid). |
| `useCalibration`   | bool    | `false`        | Apply the hard-iron/soft-iron magnetometer correction. |
| `magOffsetX/Y/Z`   | int     | `0`            | Hard-iron bias per axis (raw magnetometer counts). |
| `magScaleX/Y/Z`    | float   | `1.0`          | Soft-iron scale per axis. |
| `headingOffset`    | float   | `0`            | Manual heading correction in degrees (also used for declination). |
| `headingSmooth`    | int     | `60`           | Heading smoothing 0 (none)…100 (heavy). |

### Effects

WLED's effect engine works per *segment*, not per LED, so this usermod
emulates a small subset of the built-in effect indices on top of the base
color. Unknown indices render as solid.

| Index | Name          | Behaviour                              |
|-------|---------------|----------------------------------------|
| 0     | Static        | Solid color                            |
| 1     | Blink         | On/off at ~1.25 Hz                     |
| 2     | Breath        | Sinusoidal brightness                 |
| 8     | Rainbow       | Hue distributed across the ring       |
| 9     | Rainbow cycle | Hue rotates over time                  |
| 17    | Twinkle       | Random shimmer                         |

## Calibration

The magnetometer must be calibrated once (after mounting) to remove the
**hard-iron** offset (nearby magnets, wiring, steel in the surroundings) and
**soft-iron** distortion (unequal sensitivity / material in the magnetic
field).

### Auto-calibration (recommended)

1. Open **Config → Usermod** and make sure `sensorEnabled` is on and the
   sensor is detected (heading/pitch/roll appear on the **Info** page).
2. Start calibration:
   ```bash
   curl -X POST http://<wled-ip>/json/state \
        -d '{"MPU9250Compass":{"calibrate":true}}'
   ```
3. Hold the device level-ish and slowly rotate it through several full turns
   in a figure-of-eight pattern, tilting it in every direction. Keep it away
   from your phone, laptop, keys and any ferrous objects. **30–60 seconds** of
   movement gives a good spread of min/max values.
4. While calibrating you can watch the captured extents in the JSON state
   (`calMin`/`calMax`).
5. Stop and save the calibration:
   ```bash
   curl -X POST http://<wled-ip>/json/state \
        -d '{"MPU9250Compass":{"saveCalibration":true}}'
   ```
   (sending `{"calibrate":false}` also finalises it)
6. The computed `magOffsetX/Y/Z`, `magScaleX/Y/Z` are applied immediately and
   `useCalibration` is enabled. **Click Save in the WLED UI** to persist them
   to `cfg.json`.

### Manual calibration

1. Read raw magnetometer values from the JSON API while rotating the device:
   ```bash
   curl http://<wled-ip>/json/state
   ```
   look for `"MPU9250Compass":{"mag":[x,y,z],...}`.
2. Record the minimum and maximum of each axis over a full rotation:
   `offset_i = (max_i + min_i) / 2`.
3. For soft-iron: `scale_i = 1 / ((max_i - min_i) / 2)` normalised so the
   largest range keeps scale 1 (see the formula used by `finalizeCalibration`
   in the source).
4. Enter the values in the usermod settings and enable `useCalibration`.

### Reset calibration

```bash
curl -X POST http://<wled-ip>/json/state \
     -d '{"MPU9250Compass":{"resetCalibration":true}}'
```

## JSON API

The usermod exposes its live state under `MPU9250Compass` in `/json/state`:

```json
{
  "MPU9250Compass": {
    "sensorAvailable": true,
    "status": "ok",
    "sensorKind": 1,
    "magType": 1,
    "whoAmI": 113,
    "magWhoAmI": 72,
    "magPath": 1,
    "i2cScan": [12, 104],
    "heading": 42.5,
    "pitch": -1.2,
    "roll": 0.8,
    "acc":  [16, -12, 16000],
    "gyr":  [3, 2, -1],
    "mag":  [-120, 280, 340],
    "calibrating": false,
    "calMin": [ -180, -200, -150 ],
    "calMax": [  190,  210,  170 ]
  }
}
```

* `heading` is in degrees (0 = magnetic North, clockwise), already smoothed.
  In MPU-9250 mode it is also tilt-compensated.
* `pitch` / `roll` are in degrees (MPU-9250 mode only; `n/a` for GY-271).
* `mag` is the raw, factory-sensitivity-adjusted magnetometer reading
  (before hard/soft-iron correction).
* `calMin`/`calMax` are only present while calibrating.

**Diagnostics** (shown even when the sensor is offline — great for debugging):

| Field | Meaning |
|-------|---------|
| `status` | Plain-language reason if the sensor is offline, e.g. `"magnetometer not found - this module appears to be an MPU-6500 without a compass"`. |
| `sensorKind` | Active sensor: `1` = MPU-9250, `2` = GY-271, `0` = none. |
| `accelAvailable` | Accelerometer usable (drives the Falling Sand tilt effect). |
| `magAvailable` | Magnetometer usable (drives the compass). |
| `magType` | Magnetometer found: `1` = AK8963 (MPU-9250), `2` = HMC5883L, `3` = QMC5883L, `0` = none. |
| `whoAmI` | MPU-9250 chip ID (only in MPU mode). `113` (0x71) = genuine MPU-9250, `112` (0x70) or `104` (0x68) = clones, `0` = no reply. |
| `magWhoAmI` | AK8963 chip ID (only in MPU mode). `72` (0x48) = genuine, `0` = not found. |
| `magPath` | MPU magnetometer access: `1` = I2C bypass, `2` = internal I2C master, `0` = none. |
| `i2cScan` | Every I2C address that answered a bus scan. MPU-9250: `104` (0x68) + `12` (0x0C). GY-271: `30` (0x1E, HMC5883L) or `13` (0x0D, QMC5883L). |

Commands (POST to `/json/state`):

```json
{"MPU9250Compass":{"calibrate":true}}        // start calibration
{"MPU9250Compass":{"calibrate":false}}       // stop + apply
{"MPU9250Compass":{"saveCalibration":true}}  // apply calibration now
{"MPU9250Compass":{"resetCalibration":true}} // clear calibration
{"MPU9250Compass":{"scanI2C":true}}          // re-scan the I2C bus + re-init sensor
{"MPU9250Compass":{"reinit":true}}           // re-initialise the sensor
```

The Info page of the WLED UI also shows the live compass heading, pitch and
roll.

## Examples

### 60-LED ring compass

```ini
totalLeds   = 60
northColor  = FF0000
northEffect = 0
northSize   = 5        ; ~30° wide sector
otherColor  = 000022
otherEffect = 0
```

### 24-LED ring, blinking north sector

```ini
totalLeds   = 24
northColor  = FFFF00
northEffect = 1        ; blink
northSize   = 3
otherColor  = 220000
otherEffect = 0
```

### Rainbow background, rotating north marker

```ini
totalLeds   = 120
northColor  = FFFFFF
northEffect = 9        ; rainbow cycle
northSize   = 1        ; single "hand"
otherColor  = FF00FF
otherEffect = 8        ; rainbow
```

### Correcting mounting & magnetic declination

1. Point the direction you want to read as `0°` exactly at magnetic North
   (use a real compass or a known map bearing) and read the displayed heading
   from `/json/state`.
2. Set `headingOffset = -displayedHeading` in the usermod settings (e.g.
   displayed `8°` → `headingOffset = -8`).
3. Optionally add your local **magnetic declination**: true north = magnetic
   north + declination. With a 5° east declination the reading already
   displays `8°` when pointed at magnetic north, so
   `headingOffset = -8 + 5 = -3`.

## Troubleshooting

| Symptom                              | Fix |
|--------------------------------------|-----|
| Info page shows "sensor offline"     | Check `MPU9250Compass.i2cScan` in `/json/state`: if nothing responds, fix wiring/pins/pull-ups/power; if the sensor answers but `magWhoAmI` is 0, the magnetometer is missing (see below). |
| Nothing on the I2C scan              | Wrong SDA/SCL pins, missing pull-up resistors, no power, or address mismatch. Double-check AD0 (0x68 = GND, 0x69 = 3.3V). |
| ESP32-C3: sensor offline on GPIO 8/9 | Many ESP32-C3 boards use **GPIO8 for the onboard RGB LED** and **GPIO9 for the BOOT button** (e.g. DevKitM-1). Use other pins, e.g. GPIO4/5 or GPIO6/7. |
| Sensor answers but `magWhoAmI` = 0   | Many cheap "MPU-9250" modules are actually **MPU-6500** chips (WHO_AM_I = 0x70) with no magnetometer — a compass can't work without it. The usermod probes both the I2C bypass and the internal I2C master path before concluding this. **The accelerometer still works**, so the "Hourglass (tilt)" effect functions normally; only the compass is unavailable. |
| GY-271 mode: sensor offline          | Check the scan shows `30` (0x1E, HMC5883L) or `13` (0x0D, QMC5883L). If nothing: wiring/pins/power. If `magType` is 0 but the address appears, try `sensorType = 2` to skip MPU detection. |
| GY-271 heading wrong when tilted     | Expected: a magnetometer-only module can't measure tilt. Mount the GY-271 **level** (the MPU-9250 mode provides tilt compensation instead). |
| Heading is constant/wrong            | Run the calibration; check `useCalibration`; check `headingOffset` and the +X axis convention. |
| Heading jitters                      | Increase `headingSmooth` (up to 100). |
| North sector seems misaligned        | One LED = 360/`totalLeds` degrees; set `northSize` accordingly and use `headingOffset` to align. |
| Pin/address change has no effect     | Reboot after changing `sdaPin`/`sclPin`/`i2cAddress` (changes are applied automatically on the next loop as well). |

## Files

```
usermods/mpu9250_compass/
├── library.json                  # PlatformIO library manifest (compile-time enable)
├── platformio_override.ini       # example override to enable the usermod
├── readme.md                     # short in-tree readme
├── usermod_mpu9250_compass.h     # usermod declaration
└── usermod_mpu9250_compass.cpp   # usermod + minimal inline MPU-9250 driver
integration/
└── wled-16.0.0-integration.patch # applies the usermod onto WLED v16.0.0
```

## License

MIT — see [LICENSE](LICENSE). This usermod is not part of the official WLED
project.
