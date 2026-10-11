
#include "APDS9930Light.h"
#include <math.h>

bool APDS9930Light::begin(TwoWire* wire, uint8_t address) {
  wire_ = wire;
  address_ = address;
  uint8_t id = 0;
  if (!readReg(REG_ID, id)) return false;
  return configureDefault();
}

bool APDS9930Light::configureDefault() {
  if (!writeReg(REG_ENABLE, 0x00)) return false;
  if (!writeReg(REG_ATIME, 0xDB)) return false; // ~100 ms
  if (!setAmbientGain(1)) return false;         // 8x
  return setEnable(true, true, false, false);   // ALS only
}

bool APDS9930Light::setAmbientGain(uint8_t gain) {
  uint8_t v = 0;
  if (!readReg(REG_CONTROL, v)) return false;
  gain &= 0x03;
  v &= 0xFC;
  v |= gain;
  if (!writeReg(REG_CONTROL, v)) return false;
  gainCode_ = gain;
  return true;
}

bool APDS9930Light::setEnable(bool power, bool als, bool prox, bool waitMode) {
  uint8_t enable = 0;
  if (power) enable |= 0x01;
  if (als) enable |= 0x02;
  if (prox) enable |= 0x04;
  if (waitMode) enable |= 0x08;
  return writeReg(REG_ENABLE, enable);
}

bool APDS9930Light::readRaw(uint16_t& clear, uint16_t& ir) {
  return readReg16(REG_CDATAL, clear) && readReg16(REG_IRDATAL, ir);
}

bool APDS9930Light::readLux(float& lux) {
  uint16_t clear = 0, ir = 0;
  if (!readRaw(clear, ir)) return false;
  float visible = (float)clear - (float)ir;
  if (visible < 0.0f) visible = 0.0f;
  float gain = 8.0f;
  switch (gainCode_) {
    case 0: gain = 1.0f; break;
    case 1: gain = 8.0f; break;
    case 2: gain = 16.0f; break;
    case 3: gain = 120.0f; break;
  }
  lux = (visible * 60.0f) / (264.0f * gain);
  if (lux < 0.0f) lux = 0.0f;
  return true;
}

bool APDS9930Light::writeReg(uint8_t reg, uint8_t value) {
  wire_->beginTransmission(address_);
  wire_->write(CMD | reg);
  wire_->write(value);
  return wire_->endTransmission() == 0;
}

bool APDS9930Light::readReg(uint8_t reg, uint8_t& value) {
  wire_->beginTransmission(address_);
  wire_->write(CMD | reg);
  if (wire_->endTransmission(false) != 0) return false;
  if (wire_->requestFrom((int)address_, 1) != 1) return false;
  value = wire_->read();
  return true;
}

bool APDS9930Light::readReg16(uint8_t regL, uint16_t& value) {
  uint8_t lo = 0, hi = 0;
  if (!readReg(regL, lo)) return false;
  if (!readReg(regL + 1, hi)) return false;
  value = (uint16_t)lo | ((uint16_t)hi << 8);
  return true;
}
