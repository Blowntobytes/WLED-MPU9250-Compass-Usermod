# MPU-9250 Compass Usermod

Turns a NeoPixel strip/ring into a compass using an MPU-9250 9-axis IMU
(gyroscope + accelerometer + magnetometer) on the I2C bus.

* Computes a tilt-compensated magnetic heading from the magnetometer and
  accelerometer (pitch/roll/yaw).
* LEDs pointing towards magnetic North render with a configurable color and
  (emulated) effect; all other LEDs render with a different color/effect.
* Full magnetometer calibration (hard-iron offset + soft-iron scaling) with an
  in-place calibration routine and manual entry.
* No external libraries required - a minimal self-contained MPU-9250 driver is
  bundled. Only the Arduino `Wire` core is used.

## Wiring (ESP32 defaults)

| MPU-9250 | ESP32     |
|----------|-----------|
| VCC      | 3.3V      |
| GND      | GND       |
| SDA      | GPIO 21   |
| SCL      | GPIO 22   |

I2C address `0x68` (AD0 = GND) or `0x69` (AD0 = 3.3V). SDA/SCL pins and the
address are configurable in the usermod settings.

If the sensor is not detected, `begin()` fails gracefully: the usermod stays
disabled and the strip behaves normally.

## Enable

Add `mpu9250_compass` to the `custom_usermods` list of your
`platformio_override.ini` (see `platformio_override.ini` in this folder), or
use the stock `usermods` env which builds every usermod automatically.

Full usage, settings reference and the calibration procedure are described in
the repository README.
