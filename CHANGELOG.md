# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Versioning scheme:
* **MAJOR** (v2.0.0) — breaking changes (config keys, WLED integration).
* **MINOR** (v1.1.0, v1.2.0, ...) — new features, backwards compatible.
* **PATCH** (v1.1.1, v1.1.2, ...) — bug fixes.

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