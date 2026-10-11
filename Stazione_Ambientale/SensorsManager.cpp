#include "SensorsManager.h"

#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_AHTX0.h>
#include <DFRobot_ENS160.h>
#include "APDS9930Light.h"
#include <math.h>

TwoWire I2C_AIR = TwoWire(0);
TwoWire I2C_BMP = TwoWire(1);

static Adafruit_BMP280 bmp(&I2C_BMP);
static Adafruit_AHTX0 aht;
static DFRobot_ENS160_I2C ens53(&I2C_AIR, 0x53);
static DFRobot_ENS160_I2C ens52(&I2C_AIR, 0x52);
static DFRobot_ENS160_I2C* ens = nullptr;
static APDS9930Light apds;

static bool initBMP280() {
  if (bmp.begin(0x76)) { Serial.println("[BMP280] OK @ 0x76 su I2C_BMP"); return true; }
  if (bmp.begin(0x77)) { Serial.println("[BMP280] OK @ 0x77 su I2C_BMP"); return true; }
  Serial.println("[BMP280] non trovato");
  return false;
}

static bool initAHT21() {
  if (!aht.begin(&I2C_AIR)) {
    Serial.println("[AHT21] non trovato");
    return false;
  }
  Serial.println("[AHT21] OK su I2C_AIR");
  return true;
}

static bool initAPDS9930() {
  if (!apds.begin(&I2C_AIR, 0x39)) {
    Serial.println("[APDS-9930] non trovato");
    return false;
  }
  Serial.println("[APDS-9930] OK @ 0x39 su I2C_AIR");
  return true;
}

static bool initENS160() {
  if (ens53.begin() == NO_ERR) {
    ens = &ens53;
    ens->setPWRMode(ENS160_STANDARD_MODE);
    Serial.println("[ENS160] OK @ 0x53 su I2C_AIR");
    return true;
  }
  if (ens52.begin() == NO_ERR) {
    ens = &ens52;
    ens->setPWRMode(ENS160_STANDARD_MODE);
    Serial.println("[ENS160] OK @ 0x52 su I2C_AIR");
    return true;
  }
  Serial.println("[ENS160] non trovato");
  return false;
}

void initI2CBuses() {
  I2C_AIR.begin(PIN_I2C0_SDA, PIN_I2C0_SCL, 100000);
  I2C_BMP.begin(PIN_I2C1_SDA, PIN_I2C1_SCL, 100000);
}

static void scanBus(TwoWire& bus, const char* name) {
  Serial.printf("\n[%s] scan start\n", name);
  for (uint8_t address = 1; address < 127; address++) {
    bus.beginTransmission(address);
    if (bus.endTransmission() == 0) {
      Serial.printf("[%s] trovato 0x%02X\n", name, address);
    }
  }
  Serial.printf("[%s] scan end\n", name);
}

void scanI2CBuses() {
  scanBus(I2C_AIR, "I2C_AIR");
  scanBus(I2C_BMP, "I2C_BMP");
}

void beginSensors() {
  app.status.bmpOk = initBMP280();
  app.status.ahtOk = initAHT21();
  app.status.apdsOk = initAPDS9930();
  app.status.ensOk = initENS160();
  if (app.status.bmpOk) {
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X16,
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
  }
}

float readAmbientLux() {
  if (!app.status.apdsOk) return NAN;
  float lux = NAN;
  if (!apds.readLux(lux)) return NAN;
  return lux;
}

void evaluateAlerts() {
  app.alerts = AlertState();
  if (!app.cfg.alertsEnabled) return;

  if (!isnan(app.data.temperatureAir)) {
    app.alerts.tempLow = app.data.temperatureAir < app.thr.tempLow;
    app.alerts.tempHigh = app.data.temperatureAir > app.thr.tempHigh;
  }
  if (!isnan(app.data.humidityAir)) {
    app.alerts.humLow = app.data.humidityAir < app.thr.humLow;
    app.alerts.humHigh = app.data.humidityAir > app.thr.humHigh;
  }
  if (!isnan(app.data.pressureHpa)) {
    app.alerts.pressureLow = app.data.pressureHpa < app.thr.pressureLow;
    app.alerts.pressureHigh = app.data.pressureHpa > app.thr.pressureHigh;
  }
  app.alerts.eco2Warn = app.data.eco2 >= app.thr.eco2Warn;
  app.alerts.eco2Alarm = app.data.eco2 >= app.thr.eco2Alarm;
  app.alerts.tvocWarn = app.data.tvoc >= app.thr.tvocWarn;
  app.alerts.tvocAlarm = app.data.tvoc >= app.thr.tvocAlarm;
  app.alerts.aqiWarn = app.data.aqi >= app.thr.aqiWarn;

  String summary;
  auto add = [&](const String& s) {
    if (summary.length()) summary += " | ";
    summary += s;
    app.alerts.count++;
  };
  if (app.alerts.tempLow) add("Temp bassa");
  if (app.alerts.tempHigh) add("Temp alta");
  if (app.alerts.humLow) add("Umidita bassa");
  if (app.alerts.humHigh) add("Umidita alta");
  if (app.alerts.pressureLow) add("Pressione bassa");
  if (app.alerts.pressureHigh) add("Pressione alta");
  if (app.alerts.eco2Alarm) add("eCO2 allarme");
  else if (app.alerts.eco2Warn) add("eCO2 alto");
  if (app.alerts.tvocAlarm) add("TVOC allarme");
  else if (app.alerts.tvocWarn) add("TVOC alto");
  if (app.alerts.aqiWarn) add("AQI elevato");

  app.alerts.any = app.alerts.count > 0;
  app.alerts.summary = summary;
}

void readSensors() {
  bool changed = false;

  if (app.status.bmpOk) {
    const float p = bmp.readPressure() / 100.0f;
    if (isnan(app.data.pressureHpa) || fabsf(p - app.data.pressureHpa) >= 0.1f) {
      app.data.pressureHpa = p;
      changed = true;
    }
  }

  if (app.status.ahtOk) {
    sensors_event_t humEvent, tempEvent;
    aht.getEvent(&humEvent, &tempEvent);
    const float t = tempEvent.temperature;
    const float h = humEvent.relative_humidity;
    if (isnan(app.data.temperatureAir) || fabsf(t - app.data.temperatureAir) >= 0.1f) {
      app.data.temperatureAir = t;
      changed = true;
    }
    if (isnan(app.data.humidityAir) || fabsf(h - app.data.humidityAir) >= 0.1f) {
      app.data.humidityAir = h;
      changed = true;
    }
  }

  if (app.status.apdsOk) {
    const float rawLux = readAmbientLux();
    if (!isnan(rawLux)) {
      const float filteredLux = isnan(app.data.lux) ? rawLux : (app.data.lux * 0.75f + rawLux * 0.25f);
      if (isnan(app.data.lux) || fabsf(filteredLux - app.data.lux) >= 0.5f) {
        app.data.lux = filteredLux;
        changed = true;
      }
    }
  }

  if (app.status.ensOk && ens != nullptr) {
    if (!isnan(app.data.temperatureAir) && !isnan(app.data.humidityAir)) {
      ens->setTempAndHum(app.data.temperatureAir, app.data.humidityAir);
    }
    const uint8_t status = ens->getENS160Status();
    const uint8_t aqi = ens->getAQI();
    const uint16_t tvoc = ens->getTVOC();
    const uint16_t eco2 = ens->getECO2();
    if (status != app.data.ensStatus) { app.data.ensStatus = status; changed = true; }
    if (aqi != app.data.aqi) { app.data.aqi = aqi; changed = true; }
    if (tvoc != app.data.tvoc) { app.data.tvoc = tvoc; changed = true; }
    if (eco2 != app.data.eco2) { app.data.eco2 = eco2; changed = true; }
  }

  const String prevSummary = app.alerts.summary;
  const uint8_t prevCount = app.alerts.count;
  evaluateAlerts();
  if (app.alerts.summary != prevSummary || app.alerts.count != prevCount) {
    changed = true;
  }

  pushHistorySample(app.data);
  if (changed) {
    markDisplayValuesDirty();
    markDisplayFooterDirty();
  }
}
