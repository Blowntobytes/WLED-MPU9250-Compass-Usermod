/*
 * WLED usermod: MPU-9250 compass LED display (implementation)
 * See usermod_mpu9250_compass.h for details.
 */

#include "wled.h"
#include "usermod_mpu9250_compass.h"
#include <Wire.h>
#include <math.h>

/*
 * Minimal, self-contained MPU-9250 driver.
 *
 * The MPU-9250 contains an MPU-6500 (gyro/accel) plus an embedded AK8963
 * magnetometer. The magnetometer is exposed on the same I2C bus at address
 * 0x0C by enabling the MPU's I2C bypass. No external library is required,
 * only the Arduino Wire (TwoWire) core.
 */
namespace mpu9250 {

constexpr uint8_t AK8963_I2C_ADDR = 0x0C;

// MPU-6500 register map
constexpr uint8_t REG_WHO_AM_I     = 0x75;
constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
constexpr uint8_t REG_SMPLRT_DIV   = 0x19;
constexpr uint8_t REG_CONFIG       = 0x1A;
constexpr uint8_t REG_GYRO_CONFIG  = 0x1B;
constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
constexpr uint8_t REG_USER_CTRL    = 0x6A;
constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
constexpr uint8_t REG_PWR_MGMT_2   = 0x6C;
constexpr uint8_t REG_INT_PIN_CFG  = 0x37;

// AK8963 register map
constexpr uint8_t AK_WIA   = 0x00;
constexpr uint8_t AK_ST1   = 0x02;
constexpr uint8_t AK_HXL   = 0x03;
constexpr uint8_t AK_ST2   = 0x09;
constexpr uint8_t AK_CNTL1 = 0x0A;
constexpr uint8_t AK_CNTL2 = 0x0B;
constexpr uint8_t AK_ASAX  = 0x10;

bool writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return (Wire.endTransmission() == 0);
}

bool readRegs(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  Wire.requestFrom((uint16_t)addr, (uint8_t)len);
  uint8_t i = 0;
  while (Wire.available() && i < len) buf[i++] = Wire.read();
  return (i == len);
}

bool readReg(uint8_t addr, uint8_t reg, uint8_t &val) {
  return readRegs(addr, reg, &val, 1);
}

} // namespace mpu9250

class Mpu9250Driver {
  private:
    uint8_t _addr = 0x68;
    float   _magSens[3] = {1.0f, 1.0f, 1.0f};

    bool initAk8963() {
      using namespace mpu9250;
      uint8_t wia = 0;
      if (!readReg(AK8963_I2C_ADDR, AK_WIA, wia) || wia != 0x48) return false;
      writeReg(AK8963_I2C_ADDR, AK_CNTL2, 0x01); // soft reset
      delay(10);
      // read factory sensitivity adjustments from FUSE ROM
      writeReg(AK8963_I2C_ADDR, AK_CNTL1, 0x0F); // FUSE_ROM mode
      delay(10);
      uint8_t asa[3] = {128, 128, 128};
      if (readRegs(AK8963_I2C_ADDR, AK_ASAX, asa, 3)) {
        for (uint8_t i = 0; i < 3; i++)
          _magSens[i] = (((float)asa[i] - 128.0f) * 0.5f / 128.0f) + 1.0f;
      }
      writeReg(AK8963_I2C_ADDR, AK_CNTL1, 0x00); // power down
      delay(10);
      writeReg(AK8963_I2C_ADDR, AK_CNTL1, 0x16); // 16-bit, 100 Hz continuous
      delay(20);
      return true;
    }

  public:
    bool begin(uint8_t addr, int sda, int scl) {
      using namespace mpu9250;
      // (re-)initialise the I2C bus so pin/address changes take effect too
      Wire.begin(sda, scl);
      Wire.setClock(400000); // 400 kHz
      _addr = addr;
      uint8_t who = 0;
      if (!readReg(_addr, REG_WHO_AM_I, who)) return false;
      // genuine MPU-9250 = 0x71; tolerate common clones (0x70/0x68)
      if (who != 0x71 && who != 0x70 && who != 0x68) return false;
      // reset the device
      writeReg(_addr, REG_PWR_MGMT_1, 0x80);
      delay(100);
      // clock source = PLL with X-axis gyroscope, wake up all sensors
      writeReg(_addr, REG_PWR_MGMT_1, 0x01);
      writeReg(_addr, REG_PWR_MGMT_2, 0x00);
      // disable I2C master, then enable I2C bypass -> AK8963 at 0x0C
      writeReg(_addr, REG_USER_CTRL, 0x00);
      writeReg(_addr, REG_INT_PIN_CFG, 0x02);
      // sample rate 1 kHz / (1 + 4) = 200 Hz, DLPF 44 Hz
      writeReg(_addr, REG_SMPLRT_DIV, 0x04);
      writeReg(_addr, REG_CONFIG, 0x03);
      writeReg(_addr, REG_GYRO_CONFIG, 0x08);   // +/-500 dps
      writeReg(_addr, REG_ACCEL_CONFIG, 0x00);  // +/-2 g
      return initAk8963();
    }

    // acc/gyr raw 16-bit; mag raw 16-bit (already multiplied by factory sens)
    bool readAll(int16_t *acc, int16_t *gyr, int16_t *mag) {
      using namespace mpu9250;
      uint8_t buf[14];
      if (!readRegs(_addr, REG_ACCEL_XOUT_H, buf, 14)) return false;
      acc[0] = (int16_t)((buf[0] << 8) | buf[1]);
      acc[1] = (int16_t)((buf[2] << 8) | buf[3]);
      acc[2] = (int16_t)((buf[4] << 8) | buf[5]);
      gyr[0] = (int16_t)((buf[8]  << 8) | buf[9]);
      gyr[1] = (int16_t)((buf[10] << 8) | buf[11]);
      gyr[2] = (int16_t)((buf[12] << 8) | buf[13]);
      return readMag(mag);
    }

    bool readMag(int16_t *mag) {
      using namespace mpu9250;
      uint8_t st1 = 0;
      uint32_t start = millis();
      do {
        if (!readReg(AK8963_I2C_ADDR, AK_ST1, st1)) return false;
        if (st1 & 0x01) break; // data ready
      } while (millis() - start < 20);
      if (!(st1 & 0x01)) return false;
      uint8_t buf[7];
      if (!readRegs(AK8963_I2C_ADDR, AK_HXL, buf, 7)) return false;
      if (buf[6] & 0x08) return false; // magnetic sensor overflow (HOFL)
      mag[0] = (int16_t)((buf[1] << 8) | buf[0]);
      mag[1] = (int16_t)((buf[3] << 8) | buf[2]);
      mag[2] = (int16_t)((buf[5] << 8) | buf[4]);
      for (uint8_t i = 0; i < 3; i++)
        mag[i] = (int16_t)((float)mag[i] * _magSens[i]);
      return true;
    }
};

static Mpu9250Driver _driver;

const char Mpu9250Compass::_name[] PROGMEM = "MPU9250Compass";

/* ---------------------------------------------------------------- setup */

void Mpu9250Compass::initSensor() {
  _initAddr = i2cAddress;
  _initSda  = sdaPin;
  _initScl  = sclPin;
  sensorAvailable = sensorEnabled && _driver.begin(i2cAddress, sdaPin, sclPin);
  readErrors = 0;
  if (sensorAvailable) lastSample = 0;
}

void Mpu9250Compass::setup() {
  initialized = true;
  parseColor(northColorStr, _northColor);
  parseColor(otherColorStr, _otherColor);
  initSensor();
}

/* ---------------------------------------------------------------- loop */

void Mpu9250Compass::loop() {
  if (!initialized || !sensorEnabled) return;
  // pick up I2C pin/address changes from the settings page without reboot
  if (_initAddr != i2cAddress || _initSda != sdaPin || _initScl != sclPin) {
    initSensor();
    if (!sensorAvailable) return;
  }
  if (millis() - lastSample < 50) return; // ~20 Hz update rate
  lastSample = millis();

  if (_driver.readAll(acc, gyr, mag)) {
    readErrors = 0;
    if (!sensorAvailable) sensorAvailable = true; // sensor recovered
    updateHeading();
    if (calibrating) trackCalibration();
    strip.trigger(); // keep the overlay rendering (also animates effects)
  } else if (++readErrors >= 20) {
    // repeatedly failing reads -> disable the overlay until a retry succeeds
    sensorAvailable = false;
    readErrors = 0;
  } else if (!sensorAvailable && (millis() & 0x1FFF) == 0) {
    initSensor(); // periodic retry after sensor loss
  }
}

/* ------------------------------------------------------ heading math */

void Mpu9250Compass::updateHeading() {
  constexpr float PI_F = 3.14159265358979f;
  constexpr float ACCEL_SCALE = 2.0f / 32767.0f; // +/-2 g full scale

  // accelerometer in g
  float ax = (float)acc[0] * ACCEL_SCALE;
  float ay = (float)acc[1] * ACCEL_SCALE;
  float az = (float)acc[2] * ACCEL_SCALE;

  // hard-iron offset (+ soft-iron scaling) corrected magnetometer
  float mx = (float)mag[0] - (float)magOffset[0];
  float my = (float)mag[1] - (float)magOffset[1];
  float mz = (float)mag[2] - (float)magOffset[2];
  if (useCalibration) {
    mx *= magScale[0];
    my *= magScale[1];
    mz *= magScale[2];
  }

  // device orientation from gravity (roll/pitch)
  float r  = atan2f(ay, az);
  float p  = atan2f(-ax, sqrtf(ay * ay + az * az));

  // project the magnetic field onto the horizontal plane (tilt compensation)
  float sinp = sinf(p), cosp = cosf(p);
  float sinr = sinf(r),  cosr = cosf(r);
  float Xh = mx * cosp + mz * sinp;
  float Yh = mx * sinr * sinp + my * cosr - mz * sinr * cosp;

  // heading, degrees, clockwise from the sensor +X axis toward magnetic north
  float h = atan2f(Yh, Xh) * 180.0f / PI_F;
  h += headingOffset;
  h = fmodf(h, 360.0f);
  if (h < 0.0f) h += 360.0f;

  // exponential moving average in sine/cosine space (handles 360 wraparound)
  float alpha = (100.0f - (float)headingSmooth) / 100.0f;
  if (alpha < 0.05f) alpha = 0.05f;
  float hr = h * PI_F / 180.0f;
  _smoothCos = _smoothCos * (1.0f - alpha) + cosf(hr) * alpha;
  _smoothSin = _smoothSin * (1.0f - alpha) + sinf(hr) * alpha;

  heading = atan2f(_smoothSin, _smoothCos) * 180.0f / PI_F;
  if (heading < 0.0f) heading += 360.0f;
  pitch = p * 180.0f / PI_F;
  roll  = r * 180.0f / PI_F;
}

/* ------------------------------------------------------ calibration */

void Mpu9250Compass::startCalibration() {
  calibrating = true;
  haveCalibData = false;
  for (uint8_t i = 0; i < 3; i++) {
    calMin[i] = 32767;
    calMax[i] = -32768;
  }
}

void Mpu9250Compass::trackCalibration() {
  for (uint8_t i = 0; i < 3; i++) {
    if (mag[i] < calMin[i]) calMin[i] = mag[i];
    if (mag[i] > calMax[i]) calMax[i] = mag[i];
  }
  haveCalibData = true;
}

void Mpu9250Compass::finalizeCalibration() {
  if (!haveCalibData) return;
  // hard iron: midpoint of the min/max range
  for (uint8_t i = 0; i < 3; i++) {
    magOffset[i] = (int16_t)(((int32_t)calMax[i] + calMin[i]) / 2);
  }
  // soft iron: scale every axis so all ranges become equal to the largest
  float radius[3];
  float maxRadius = 0.0f;
  for (uint8_t i = 0; i < 3; i++) {
    radius[i] = ((float)calMax[i] - calMin[i]) / 2.0f;
    if (radius[i] > maxRadius) maxRadius = radius[i];
  }
  for (uint8_t i = 0; i < 3; i++) {
    magScale[i] = (radius[i] > 0.0f) ? (maxRadius / radius[i]) : 1.0f;
  }
  useCalibration = true;
  calibrating = false;
  haveCalibData = false;
}

void Mpu9250Compass::resetCalibration() {
  calibrating = false;
  haveCalibData = false;
  useCalibration = false;
  for (uint8_t i = 0; i < 3; i++) { magOffset[i] = 0; magScale[i] = 1.0f; }
}

/* ------------------------------------------------------ colors & fx */

void Mpu9250Compass::parseColor(const char *in, uint32_t &out) {
  byte rgb[4] = {0, 0, 0, 0};
  if (in && colorFromHexString(rgb, in)) {
    out = RGBW32(rgb[0], rgb[1], rgb[2], 0);
  } else {
    out = RGBW32(255, 0, 0, 0);
  }
}

uint32_t Mpu9250Compass::scaleColor(uint32_t c, uint8_t scale) {
  return RGBW32(scale8(R(c), scale), scale8(G(c), scale),
                scale8(B(c), scale), scale8(W(c), scale));
}

/*
 * Per-LED effect emulation. WLED's real effect engine works per segment,
 * not per LED, so this usermod emulates a small subset of the built-in
 * effect indices on top of the configured base color. Any other index
 * renders as solid.
 *
 *   0  Static (solid)             (FX_MODE_STATIC)
 *   1  Blink                      (FX_MODE_BLINK)
 *   2  Breath                     (FX_MODE_BREATH)
 *   8  Rainbow                    (FX_MODE_RAINBOW)
 *   9  Rainbow cycle              (FX_MODE_RAINBOW_CYCLE)
 *  17  Twinkle                    (FX_MODE_TWINKLE)
 */
uint32_t Mpu9250Compass::applyEffect(uint32_t base, uint8_t effect,
                                     uint16_t led, uint16_t total,
                                     uint32_t now) {
  switch (effect) {
    case 1: { // Blink
      return ((now / 400) & 1U) ? base : 0U;
    }
    case 2: { // Breath
      int16_t s = (int16_t)sin8_t(uint8_t(now >> 4)) - 128;
      int16_t b = 128 + 2 * s;
      if (b < 0) b = 0; else if (b > 255) b = 255;
      return scaleColor(base, (uint8_t)b);
    }
    case 8:   // Rainbow: hue distributed over the ring
    case 9: { // Rainbow cycle: hue rotates with time
      uint8_t hue = (effect == 8)
        ? (uint8_t)((uint32_t)led * 255u / (total ? total : 1u))
        : uint8_t(now >> 2);
      uint8_t rgb[4] = {0, 0, 0, 0};
      hsv2rgb_rainbow((uint16_t)hue << 8, 255, 255, rgb, false);
      return RGBW32(rgb[0], rgb[1], rgb[2], 0);
    }
    case 17: { // Twinkle: random shimmer
      return scaleColor(base, hw_random8(32, 256));
    }
    default:
      return base;
  }
}

/* ------------------------------------------------------ overlay draw */

void Mpu9250Compass::handleOverlayDraw() {
  if (!sensorEnabled || !sensorAvailable) return;

  uint16_t ring = totalLeds;
  uint16_t stripLen = strip.getLengthTotal();
  if (ring == 0 || ring > stripLen) ring = stripLen;
  if (ring == 0) return;

  // LED whose angular position corresponds to the current heading
  float step = 360.0f / (float)ring;
  uint16_t northIdx = (uint16_t)(heading / step + 0.5f) % ring;

  uint32_t now = millis();

  for (uint16_t i = 0; i < ring; i++) {
    int32_t d = (int32_t)i - (int32_t)northIdx;
    if (d < 0) d = -d;
    if (d > (int32_t)ring - d) d = (int32_t)ring - d;

    bool isNorth = ((uint32_t)d * 2 <= (uint32_t)northSize);
    uint32_t base = isNorth ? _northColor : _otherColor;
    uint8_t  fx   = isNorth ? northEffect : otherEffect;

    strip.setPixelColor(i, applyEffect(base, fx, i, ring, now));
  }
}

/* ------------------------------------------------------ config */

void Mpu9250Compass::addToConfig(JsonObject& root) {
  JsonObject top = root.createNestedObject(FPSTR(_name));
  top["sensorEnabled"] = sensorEnabled;
  top["sdaPin"]  = sdaPin;
  top["sclPin"]  = sclPin;
  top["i2cAddress"] = i2cAddress;
  top["totalLeds"]  = totalLeds;
  top["northColor"] = northColorStr;
  top["northEffect"] = northEffect;
  top["northSize"]  = northSize;
  top["otherColor"] = otherColorStr;
  top["otherEffect"] = otherEffect;
  top["useCalibration"] = useCalibration;
  top["magOffsetX"] = magOffset[0];
  top["magOffsetY"] = magOffset[1];
  top["magOffsetZ"] = magOffset[2];
  top["magScaleX"]  = magScale[0];
  top["magScaleY"]  = magScale[1];
  top["magScaleZ"]  = magScale[2];
  top["headingOffset"] = headingOffset;
  top["headingSmooth"] = headingSmooth;
}

bool Mpu9250Compass::readFromConfig(JsonObject& root) {
  JsonObject top = root[FPSTR(_name)];
  bool complete = !top.isNull();

  complete &= getJsonValue(top["sensorEnabled"], sensorEnabled, true);
  complete &= getJsonValue(top["sdaPin"],  sdaPin,  MPU9250_DEFAULT_SDA);
  complete &= getJsonValue(top["sclPin"],  sclPin,  MPU9250_DEFAULT_SCL);
  complete &= getJsonValue(top["i2cAddress"], i2cAddress, (uint8_t)0x68);
  complete &= getJsonValue(top["totalLeds"],   totalLeds, (uint16_t)60);
  complete &= getJsonValue(top["northEffect"], northEffect, (uint8_t)0);
  complete &= getJsonValue(top["northSize"],   northSize,   (uint16_t)3);
  complete &= getJsonValue(top["otherEffect"], otherEffect, (uint8_t)0);
  complete &= getJsonValue(top["useCalibration"], useCalibration, false);
  complete &= getJsonValue(top["magOffsetX"], magOffset[0], (int16_t)0);
  complete &= getJsonValue(top["magOffsetY"], magOffset[1], (int16_t)0);
  complete &= getJsonValue(top["magOffsetZ"], magOffset[2], (int16_t)0);
  complete &= getJsonValue(top["magScaleX"],  magScale[0],  1.0f);
  complete &= getJsonValue(top["magScaleY"],  magScale[1],  1.0f);
  complete &= getJsonValue(top["magScaleZ"],  magScale[2],  1.0f);
  complete &= getJsonValue(top["headingOffset"], headingOffset, 0.0f);
  complete &= getJsonValue(top["headingSmooth"], headingSmooth, (uint8_t)60);

  if (top["northColor"].is<const char*>())
    strncpy(northColorStr, top["northColor"].as<const char*>(), 7);
  else
    complete = false;
  if (top["otherColor"].is<const char*>())
    strncpy(otherColorStr, top["otherColor"].as<const char*>(), 7);
  else
    complete = false;

  return complete;
}

/* ------------------------------------------------------ JSON API */

void Mpu9250Compass::addToJsonState(JsonObject& obj) {
  if (!sensorEnabled) return;
  JsonObject top = obj.createNestedObject(FPSTR(_name));
  top["sensorAvailable"] = sensorAvailable;
  if (sensorAvailable) {
    top["heading"] = heading;
    top["pitch"]   = pitch;
    top["roll"]    = roll;
    JsonArray a = top.createNestedArray("acc");
    a.add(acc[0]); a.add(acc[1]); a.add(acc[2]);
    JsonArray g = top.createNestedArray("gyr");
    g.add(gyr[0]); g.add(gyr[1]); g.add(gyr[2]);
    JsonArray m = top.createNestedArray("mag");
    m.add(mag[0]); m.add(mag[1]); m.add(mag[2]);
    top["calibrating"] = calibrating;
    if (calibrating && haveCalibData) {
      JsonArray mn = top.createNestedArray("calMin");
      mn.add(calMin[0]); mn.add(calMin[1]); mn.add(calMin[2]);
      JsonArray mx = top.createNestedArray("calMax");
      mx.add(calMax[0]); mx.add(calMax[1]); mx.add(calMax[2]);
    }
  }
}

void Mpu9250Compass::readFromJsonState(JsonObject& obj) {
  JsonObject top = obj[FPSTR(_name)];
  if (top.isNull()) return;

  if (top["calibrate"]) {
    bool c = top["calibrate"].as<bool>();
    if (c && !calibrating) startCalibration();
    else if (!c && calibrating) finalizeCalibration();
  }
  if (top["saveCalibration"] | false) finalizeCalibration();
  if (top["resetCalibration"] | false) resetCalibration();
}

void Mpu9250Compass::addToJsonInfo(JsonObject& root) {
  if (!sensorEnabled) return;
  JsonObject user = root["u"];
  if (user.isNull()) user = root.createNestedObject("u");

  JsonArray hInfo = user.createNestedArray("Compass heading");
  JsonArray pInfo = user.createNestedArray("Pitch");
  JsonArray rInfo = user.createNestedArray("Roll");
  if (!sensorAvailable) {
    hInfo.add(F("sensor offline"));
    hInfo.add(F(""));
    pInfo.add(F(""));
    pInfo.add(F(""));
    rInfo.add(F(""));
    rInfo.add(F(""));
    return;
  }
  hInfo.add(heading);
  hInfo.add(" deg");
  pInfo.add(pitch);
  pInfo.add(" deg");
  rInfo.add(roll);
  rInfo.add(" deg");
}

static Mpu9250Compass mpu9250_compass;
REGISTER_USERMOD(mpu9250_compass);
