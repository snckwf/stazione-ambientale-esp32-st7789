
#pragma once
#include <Arduino.h>
#include <Wire.h>

class APDS9930Light {
public:
  bool begin(TwoWire* wire = &Wire, uint8_t address = 0x39);
  bool readRaw(uint16_t& clear, uint16_t& ir);
  bool readLux(float& lux);

private:
  bool configureDefault();
  bool writeReg(uint8_t reg, uint8_t value);
  bool readReg(uint8_t reg, uint8_t& value);
  bool readReg16(uint8_t regL, uint16_t& value);
  bool setAmbientGain(uint8_t gain);
  bool setEnable(bool power, bool als, bool prox, bool waitMode = false);

  TwoWire* wire_ = &Wire;
  uint8_t address_ = 0x39;
  uint8_t gainCode_ = 1;

  static constexpr uint8_t CMD = 0x80;
  static constexpr uint8_t REG_ENABLE  = 0x00;
  static constexpr uint8_t REG_ATIME   = 0x01;
  static constexpr uint8_t REG_CONTROL = 0x0F;
  static constexpr uint8_t REG_ID      = 0x12;
  static constexpr uint8_t REG_CDATAL  = 0x14;
  static constexpr uint8_t REG_IRDATAL = 0x16;
};
