#pragma once

/*
 * WLED usermod: MPU-9250 / GY-271 compass LED display
 *
 * Turns a NeoPixel strip/ring into a compass. Two sensor options:
 *   - MPU-9250 9-axis IMU (gyro + accel + magnetometer) on the I2C bus.
 *     Computes a tilt-compensated magnetic heading from the magnetometer
 *     and accelerometer data.
 *   - GY-271 magnetometer breakout (HMC5883L or QMC5883L). Simpler 2-axis
 *     heading (module must be held level), no tilt compensation needed.
 *
 * The LEDs that point towards magnetic North render in a configurable
 * color/effect, all remaining LEDs render in a different configurable
 * color/effect.
 *
 * WLED version: 16.0.0
 *
 * Wiring (ESP32 defaults): VCC -> 3.3V, GND -> GND, SDA -> 21, SCL -> 22.
 * MPU-9250 is detected at I2C address 0x68 (AD0=0) or 0x69 (AD0=1).
 * GY-271 is auto-detected: HMC5883L at 0x1E or QMC5883L at 0x0D.
 * begin() fails gracefully: if no sensor is found the usermod stays
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
    uint8_t  sensorType      = 0;       // 0 = auto, 1 = MPU-9250, 2 = GY-271 (HMC5883L/QMC5883L)
    uint8_t  tiltAxis        = 0;       // tilt effects: 0 = both, 1 = left/right only, 2 = forward/back only
    uint8_t  magMap          = 0;       // GY-271 heading axis pair: 0=XY 1=YX 2=ZX 3=XZ 4=ZY 5=YZ
    bool     useCalibration  = false;   // apply hard/soft iron correction
    int16_t  magOffset[3]    = {0, 0, 0};
    float    magScale[3]     = {1.0f, 1.0f, 1.0f};
    float    headingOffset   = 0.0f;    // manual heading correction, degrees

    // ---- runtime state ----
    bool     sensorAvailable = false;
    bool     initialized     = false;
    uint8_t  _sensorKind     = 0;       // 0 = none, 1 = MPU-9250, 2 = GY-271
    bool     _accelOK        = false;   // accelerometer data usable (tilt effects)
    bool     _magOK          = false;   // magnetometer usable (compass)
    float    _gx = 0.0f, _gy = 1.0f;    // gravity direction on the screen plane (for tilt effects)
    float    _effTilt = 0.0f;            // axis-selected 1D tilt (-1..1) shared with WLED effects
    float    _rawGX = 0.0f, _rawGY = 0.0f; // continuous raw gravity on X/Y (in g), no dead zone - bubble level
    uint8_t  _initAddr       = 0;
    int      _initSda        = -1;
    int      _initScl        = -1;
    uint32_t lastSample      = 0;
    uint8_t  readErrors      = 0;
    int16_t  acc[3], gyr[3], mag[3];
    float    heading = 0.0f;   // tilt compensated, degrees 0..360 (smoothed)
    float    headingRaw = 0.0f; // heading before the internal smoothing (for the effect / diagnostics)
    float    pitch   = 0.0f;   // degrees
    float    roll    = 0.0f;   // degrees
    float    _smoothCos = 1.0f;
    float    _smoothSin = 0.0f;

    // ---- calibration state ----
    bool     calibrating   = false;
    bool     haveCalibData = false;
    int16_t  calMin[3] = {0, 0, 0};
    int16_t  calMax[3] = {0, 0, 0};

    // ---- diagnostics ----
    uint8_t  whoAmI    = 0;      // MPU-6500 WHO_AM_I value (0x71 genuine MPU-9250, 0x70/0x68 clones, 0 = no reply)
    uint8_t  magWhoAmI = 0;      // AK8963 WIA value (0x48 genuine magnetometer, 0 = not found)
    uint8_t  magPath   = 0;      // MPU magnetometer access: 0=none, 1=I2C bypass, 2=I2C master
    uint8_t  magType   = 0;      // magnetometer type: 0=none, 1=AK8963, 2=HMC5883L, 3=QMC5883L
    uint8_t  scanResults[117] = {};
    uint8_t  scanCount = 0;      // number of I2C addresses that answered the bus scan

    void initSensor();
    void scanI2CBus();
    const char* statusText() const;
    void updateHeading();
    void trackCalibration();
    void finalizeCalibration();
    void startCalibration();
    void resetCalibration();

  public:
    void setup() override;
    void loop() override;
    void addToConfig(JsonObject& root) override;
    bool readFromConfig(JsonObject& root) override;
    void addToJsonState(JsonObject& obj) override;
    void readFromJsonState(JsonObject& obj) override;
    void addToJsonInfo(JsonObject& obj) override;
    uint16_t getId() override { return USERMOD_ID_MPU9250_COMPASS; }
    bool getUMData(um_data_t **data) override;

    // tilt data used by the registered "Falling Sand" effect
    // (gx, gy) = unit vector of gravity projected on the display plane
    inline float tiltGX() const { return _gx; }
    inline float tiltGY() const { return _gy; }
    inline bool  tiltAvailable() const { return _accelOK; }
    inline uint8_t tiltAxisSel() const { return tiltAxis; }
    // continuous raw gravity (in g, no dead zone) for the bubble level
    inline float tiltRawX() const { return _rawGX; }
    inline float tiltRawY() const { return _rawGY; }
    // smoothed magnetometer heading (degrees 0..360) for the compass effect
    inline float compassHeading() const { return heading; }
    // raw (unsmoothed) heading - the Compass effect uses this so its own
    // smoothness slider is the only smoothing
    inline float compassHeadingRaw() const { return headingRaw; }
};
