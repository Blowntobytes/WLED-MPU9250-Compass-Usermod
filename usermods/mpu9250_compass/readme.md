# MPU-9250 / GY-271 Compass Usermod

Turns a NeoPixel strip/ring into a compass. Two sensor options, selectable in
settings (`sensorType`):

* **MPU-9250** 9-axis IMU (gyroscope + accelerometer + magnetometer) on the
  I2C bus - computes a **tilt-compensated** magnetic heading from the
  magnetometer and accelerometer (pitch/roll/yaw).
* **GY-271** magnetometer breakout (HMC5883L or QMC5883L) - classic 2-axis
  heading, accurate when the module is mounted level.

Features:

* LEDs pointing towards magnetic North render with a configurable color and
  (emulated) effect; all other LEDs render with a different color/effect.
* Full magnetometer calibration (hard-iron offset + soft-iron scaling) with an
  in-place calibration routine and manual entry.
* Auto-detection: MPU-9250 (address 0x68/0x69), HMC5883L (0x1E) or QMC5883L
  (0x0D) are recognised automatically.
* No external libraries required - minimal self-contained drivers are bundled.
  Only the Arduino `Wire` core is used.

## Wiring (ESP32 defaults)

| MPU-9250 | GY-271  | ESP32     |
|----------|---------|-----------|
| VCC      | VCC     | 3.3V      |
| GND      | GND     | GND       |
| SDA      | SDA     | GPIO 21   |
| SCL      | SCL     | GPIO 22   |

MPU-9250 I2C address `0x68` (AD0 = GND) or `0x69` (AD0 = 3.3V). GY-271 is
auto-detected at `0x1E` (HMC5883L) or `0x0D` (QMC5883L). SDA/SCL pins are
configurable in the usermod settings.

If no sensor is detected, `begin()` fails gracefully: the usermod stays
disabled and the strip behaves normally.

## Enable

Add `mpu9250_compass` to the `custom_usermods` list of your
`platformio_override.ini` (see `platformio_override.ini` in this folder), or
use the stock `usermods` env which builds every usermod automatically.

Full usage, settings reference and the calibration procedure are described in
the repository README.