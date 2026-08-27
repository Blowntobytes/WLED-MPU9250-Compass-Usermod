#pragma once

/*
 * WLED usermod: MPU-9250 compass LED display
 *
 * Turns a NeoPixel strip/ring into a compass using an MPU-9250 9-axis
 * IMU (gyro + accel + magnetometer) on the I2C bus.
 *
 * A tilt-compensated magnetic heading is computed from the magnetometer
 * and accelerometer data. The LEDs that point towards magnetic North
 * render in a configurable color/effect, all remaining LEDs render in a
 * different configurable color/effect.
 *
 * WLED version: 16.0.0
 *
 * Wiring (ESP32 defaults): VCC -> 3.3V, GND -> GND, SDA -> 21, SCL -> 22.
 * The sensor is detected at I2C address 0x68 (AD0=0) or 0x69 (AD0=1).
 * begin() fails gracefully: if the sensor is not found the usermod stays
 * disabled and the strip behaves normally.
 *
 * Enable in your build by adding "mpu9250_compass" to the custom_usermods
 * list of your platformio_override.ini, e.g.:
 *
 *   [env:my_esp32]
 *   extends = env:esp32dev
 *   custom_usermods = mpu9250_compass
 *
 * or, when using the stock "usermods" env, it is picked up automatically
 * (custom_usermods = *).
 */

#include "wled.h"

#if defined(ARDUINO_ARCH_ESP32)
  #define MPU9250_DEFAULT_SDA 21
  #define MPU9250_DEFAULT_SCL 22
#else
  #define MPU9250_DEFAULT_SDA 4
  #define MPU9250_DEFAULT_SCL 5
#endif

class Mpu9250Compass : public Usermod {
  private:
    static const char _name[];

    // ---- persistent settings (cfg.json / usermod settings UI) ----
    bool     sensorEnabled   = true;
    int      sdaPin          = MPU9250_DEFAULT_SDA;
    int      sclPin          = MPU9250_DEFAULT_SCL;
    uint8_t  i2cAddress      = 0x68;
    uint16_t totalLeds       = 60;      // size of the compass ring (LEDs)
    char     northColorStr[8] = "FF0000";  // RRGGBB
    char     otherColorStr[8] = "0022FF";  // RRGGBB
    uint8_t  northEffect     = 0;       // 0 = solid, see applyEffect()
    uint8_t  otherEffect     = 0;
    uint16_t northSize       = 3;       // number of LEDs in the North sector
    bool     useCalibration  = false;   // apply hard/soft iron correction
    int16_t  magOffset[3]    = {0, 0, 0};
    float    magScale[3]     = {1.0f, 1.0f, 1.0f};
    float    headingOffset   = 0.0f;    // manual heading correction, degrees
    uint8_t  headingSmooth   = 60;      // 0 (none) .. 100 (heavy) EMA smoothing

    // ---- runtime state ----
    bool     sensorAvailable = false;
    bool     initialized     = false;
    uint8_t  _initAddr       = 0;
    int      _initSda        = -1;
    int      _initScl        = -1;
    uint32_t lastSample      = 0;
    uint8_t  readErrors      = 0;
    int16_t  acc[3], gyr[3], mag[3];
    float    heading = 0.0f;   // tilt compensated, degrees 0..360
    float    pitch   = 0.0f;   // degrees
    float    roll    = 0.0f;   // degrees
    uint32_t _northColor = 0xFF0000;
    uint32_t _otherColor = 0x0022FF;
    float    _smoothCos = 1.0f;
    float    _smoothSin = 0.0f;

    // ---- calibration state ----
    bool     calibrating   = false;
    bool     haveCalibData = false;
    int16_t  calMin[3] = {0, 0, 0};
    int16_t  calMax[3] = {0, 0, 0};

    void initSensor();
    void updateHeading();
    void trackCalibration();
    void finalizeCalibration();
    void startCalibration();
    void resetCalibration();
    void parseColor(const char *in, uint32_t &out);
    static uint32_t scaleColor(uint32_t c, uint8_t scale);
    static uint32_t applyEffect(uint32_t base, uint8_t effect, uint16_t led,
                                uint16_t total, uint32_t now);

  public:
    void setup() override;
    void loop() override;
    void handleOverlayDraw() override;
    void addToConfig(JsonObject& root) override;
    bool readFromConfig(JsonObject& root) override;
    void addToJsonState(JsonObject& obj) override;
    void readFromJsonState(JsonObject& obj) override;
    void addToJsonInfo(JsonObject& obj) override;
    uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }
};
