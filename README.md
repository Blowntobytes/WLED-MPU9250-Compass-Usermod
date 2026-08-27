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
   │   .            .   .          │   .  = remaining LEDs   (otherColor)
   │   .     MPU-9250    .         │
   │   .    (I2C)        .         │
   │    .            .  .          │
   │     .  .  .  .  .  .          │
   └───────────────────────────────┘
```

## Features

* **Tilt-compensated heading** computed from the magnetometer and
  accelerometer (pitch / roll / yaw), so the compass works even when the
  device is not perfectly level.
* **North sector display**: a configurable number of LEDs around the heading
  render in `northColor` + `northEffect`, the rest render in `otherColor` +
  `otherEffect`.
* **Full magnetometer calibration** built in: hard-iron offsets (bias) and
  soft-iron scaling (per-axis gain), either auto-computed with the on-device
  calibration routine or entered manually.
* **Per-LED effect emulation**: solid, blink, breath, rainbow, rainbow cycle
  and twinkle on either group of LEDs (see [Effects](#effects)).
* **JSON API** integration: live heading/pitch/roll and raw sensor data in
  `/json/state`, calibration commands, and sensor readout on the Info page.
* **No external libraries**: a minimal self-contained MPU-9250 driver is
  bundled; it only uses the Arduino `Wire` core. No conflicts with WLED's
  bundled libraries.
* **Graceful failure**: if the sensor is not detected, `begin()` fails
  gracefully, the usermod stays disabled and your strip behaves normally.

## How it works

* The MPU-9250 embeds an MPU-6500 (accel + gyro) and an AK8963 magnetometer.
  The usermod enables the MPU's I2C bypass so the AK8963 is reachable on the
  same bus at address `0x0C`.
* Gravity is estimated from the accelerometer and used to derive roll/pitch.
* The raw magnetic field is projected onto the horizontal plane using
  roll/pitch (**tilt compensation**), then the magnetic heading is
  `atan2(Yh, Xh)`.
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

## Hardware & wiring

| MPU-9250 (GY-9250 / GY-91) | ESP32   | ESP8266  |
|----------------------------|---------|----------|
| VCC                        | 3.3V    | 3.3V     |
| GND                        | GND     | GND      |
| SDA                        | GPIO 21 | GPIO 4   |
| SCL                        | GPIO 22 | GPIO 5   |
| AD0 (address select)       | GND -> 0x68, 3.3V -> 0x69 | | 

* Supply voltage 3–5 V (the module's on-board regulators handle 3.3 V logic;
  do **not** feed 5 V into SDA/SCL).
* Pull-up resistors to 3.3 V on SDA/SCL are required (4.7 kΩ typical); most
  breakout boards already include them.
* SDA/SCL pins and the I2C address (`0x68` / `0x69`) are configurable in the
  usermod settings.
* The ring's first `totalLeds` LEDs (starting at LED 0) form the compass.

## Integration with WLED 16.0.0

The usermod is **compile-time optional**: it is only built when it appears in
the `custom_usermods` list of your PlatformIO environment, so the stock WLED
build is completely unaffected. No core WLED files are modified.

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
| `sdaPin`           | int     | `21` (ESP32)   | I2C SDA pin. |
| `sclPin`           | int     | `22` (ESP32)   | I2C SCL pin. |
| `i2cAddress`       | int     | `104` (0x68)   | MPU-9250 I2C address; `104` = 0x68, `105` = 0x69. |
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

* `heading` is in degrees (0 = magnetic North, clockwise), already
  tilt-compensated and smoothed.
* `pitch` / `roll` are in degrees.
* `mag` is the raw, factory-sensitivity-adjusted magnetometer reading
  (before hard/soft-iron correction).
* `calMin`/`calMax` are only present while calibrating.

Commands (POST to `/json/state`):

```json
{"MPU9250Compass":{"calibrate":true}}        // start calibration
{"MPU9250Compass":{"calibrate":false}}       // stop + apply
{"MPU9250Compass":{"saveCalibration":true}}  // apply calibration now
{"MPU9250Compass":{"resetCalibration":true}} // clear calibration
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
| Info page shows "sensor offline"     | Check wiring (SDA/SCL swapped?), pull-ups, supply voltage, and `i2cAddress` (0x68 vs 0x69). |
| No I2C devices found                 | Verify with a scanner sketch that the MPU-9250 answers at 0x68/0x69 and the AK8963 at 0x0C. |
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