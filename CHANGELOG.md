# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Versioning scheme:
* **MAJOR** (v2.0.0) — breaking changes (config keys, WLED integration).
* **MINOR** (v1.1.0, v1.2.0, ...) — new features, backwards compatible.
* **PATCH** (v1.1.1, v1.1.2, ...) — bug fixes.

## [1.4.0] - 2026-08-28

### Added
- New tilt-driven **"Bubble Level"** effect: a spirit level on the strip. A
  configurable-size bubble floats opposite gravity and sits centred when the
  device is level. Bubble colour uses the segment's **Fx** slot and the
  background uses the **Bg** slot (picked from the colour picker screen).
  - Effect speed = responsiveness, Effect intensity = bubble size.

## [1.3.0] - 2026-08-28

### Changed
- **Falling Sand** (1D) reworked to the confirmed-good behaviour: the lit sand
  pile sheds one pixel at a time from the side opposite gravity, the grain
  falls across the strip, and the low side accumulates one pixel at a time.
  Piles stay contiguous (no gaps).
- Default drip speed is now ~50% faster.
- New **Flow** slider (Custom 1) shortens the drip pause so several grains are
  in flight at once, making the sand pour like a stream.
- Level the device to stop the flow; reverse the tilt to drain the sand back.

## [1.2.0] - 2026-08-28

### Added
- New custom WLED effect **"Falling Sand"** (tilt-driven):
  - **1D strips**: a single contiguous bag of sand (no gaps) that slides
    toward the low end one pixel at a time under tilt. **Effect speed** sets
    the pause between steps, **Flow** (Custom 1) sets pixels per step
    (default 1 = one pixel at a time), **Intensity** sets the bag size.
  - **2D matrices**: a falling-sand grid that flips over (falls the other
    way) when full instead of vanishing.
  - Sand is coloured from the segment palette.
- Accelerometer usable **without** a magnetometer — tilt effects work on a
  plain MPU-6500 (common "fake MPU-9250" modules).
- `tiltAxis` setting: 0 = both axes, 1 = left/right only, 2 = forward/back only.
- `overlayEnabled` setting to disable the compass overlay (needs a magnetometer).
- Diagnostics: `accelAvailable`, `magAvailable`, `tiltAxis`, `tiltX`, `tiltY`.

## [1.1.0] - 2026-08-28

### Added
- GY-271 magnetometer support (HMC5883L and QMC5883L) via the new `sensorType`
  setting (0 = auto, 1 = MPU-9250, 2 = GY-271). GY-271 uses a simple 2-axis
  heading and must be mounted level (no accelerometer available).
- I2C diagnostics exposed in `/json/state`, even when the sensor is offline:
  `status`, `sensorKind`, `magType`, `whoAmI`, `magWhoAmI`, `magPath`, `i2cScan`.
- Internal I2C-master magnetometer access as an additional path/probe alongside
  I2C bypass (helps genuine modules where bypass is unavailable).
- `scanI2C` and `reinit` JSON state commands to re-check the bus without rebooting.

### Changed
- I2C clock lowered to 100 kHz for reliable operation with breakout modules and
  long wires.

## [1.0.0] - 2026-08-28

### Added
- Initial release: MPU-9250 9-axis compass LED display usermod for WLED 16.0.0
  (tilt-compensated heading, North-sector LED rendering, calibration, JSON API).