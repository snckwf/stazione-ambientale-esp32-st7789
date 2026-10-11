#include "AppConfig.h"
#include <Preferences.h>
#include <time.h>
#include <math.h>

AppState app;
static Preferences prefs;

String formatUptime(uint32_t ms) {
  uint32_t sec = ms / 1000UL;
  const uint32_t d = sec / 86400UL;
  sec %= 86400UL;
  const uint32_t h = sec / 3600UL;
  sec %= 3600UL;
  const uint32_t m = sec / 60UL;
  sec %= 60UL;
  char buf[32];
  snprintf(buf, sizeof(buf), "%lu g %02lu:%02lu:%02lu", (unsigned long)d, (unsigned long)h, (unsigned long)m, (unsigned long)sec);
  return String(buf);
}

String aqiText(uint8_t aqi) {
  switch (aqi) {
    case 1: return "Ottima";
    case 2: return "Buona";
    case 3: return "Media";
    case 4: return "Scarsa";
    case 5: return "Pessima";
    default: return "--";
  }
}

String ensStatusText(uint8_t st) {
  switch (st) {
    case 0: return "Normale";
    case 1: return "Warm-up";
    case 2: return "Startup";
    case 3: return "Non valido";
    default: return "Sconosciuto";
  }
}

String boolTxt(bool v) { return v ? "OK" : "ERR"; }

String currentPageName() {
  switch (app.currentPage) {
    case PAGE_HOME: return "Home";
    case PAGE_AIR: return "Aria";
    case PAGE_TRENDS: return "Grafici";
    case PAGE_SYSTEM: return "Sistema";
    default: return "?";
  }
}

String sensorStateText() {
  return String("BMP ") + boolTxt(app.status.bmpOk) +
         " · AHT " + boolTxt(app.status.ahtOk) +
         " · ENS " + boolTxt(app.status.ensOk) +
         " · APDS " + boolTxt(app.status.apdsOk);
}

String luxStateText() {
  if (!app.status.apdsOk || isnan(app.data.lux)) return "Lux n/d";
  return String((double)app.data.lux, 0) + " lx";
}

String dimmerModeText(uint8_t mode) {
  switch (mode) {
    case DIMMER_AUTO: return "Auto";
    case DIMMER_DAY: return "Giorno";
    case DIMMER_NIGHT: return "Notte";
    default: return "?";
  }
}

String alertSummary() {
  if (!app.alerts.any) return "Nessun allarme";
  return app.alerts.summary.length() ? app.alerts.summary : String("Allarmi attivi");
}

static void loadThresholds() {
  app.thr.tempLow = prefs.getFloat("tLow", 18.0f);
  app.thr.tempHigh = prefs.getFloat("tHigh", 27.0f);
  app.thr.humLow = prefs.getFloat("hLow", 35.0f);
  app.thr.humHigh = prefs.getFloat("hHigh", 65.0f);
  app.thr.pressureLow = prefs.getFloat("pLow", 995.0f);
  app.thr.pressureHigh = prefs.getFloat("pHigh", 1025.0f);
  app.thr.eco2Warn = prefs.getUShort("eco2W", 900);
  app.thr.eco2Alarm = prefs.getUShort("eco2A", 1400);
  app.thr.tvocWarn = prefs.getUShort("tvocW", 350);
  app.thr.tvocAlarm = prefs.getUShort("tvocA", 1200);
  app.thr.aqiWarn = prefs.getUChar("aqiW", 3);
}

void loadConfig() {
  prefs.begin("stazamb", true);
  app.cfg.autoRotateEnabled = prefs.getBool("autoRot", true);
  app.cfg.pageIntervalSec = prefs.getUShort("pageSec", DEFAULT_PAGE_SEC);
  app.cfg.sensorIntervalSec = prefs.getUShort("sensSec", DEFAULT_SENSOR_SEC);
  app.cfg.dimmerEnabled = prefs.getBool("dimEn", true);
  app.cfg.dimmerMode = prefs.getUChar("dimMode", DIMMER_AUTO);
  app.cfg.dayStartHour = prefs.getUChar("dayHr", 7);
  app.cfg.nightStartHour = prefs.getUChar("nightHr", 22);
  app.cfg.startPage = prefs.getUChar("startPg", 0);
  app.cfg.alertsEnabled = prefs.getBool("alertsEn", true);
  app.cfg.historyEnabled = prefs.getBool("histEn", true);
  app.cfg.useLuxForAuto = prefs.getBool("luxAuto", true);
  app.cfg.luxNightThreshold = prefs.getFloat("luxNight", 8.0f);
  app.cfg.luxDayThreshold = prefs.getFloat("luxDay", 20.0f);
  loadThresholds();
  prefs.end();

  if (app.cfg.pageIntervalSec < 3) app.cfg.pageIntervalSec = DEFAULT_PAGE_SEC;
  if (app.cfg.sensorIntervalSec < 1) app.cfg.sensorIntervalSec = DEFAULT_SENSOR_SEC;
  if (app.cfg.dayStartHour > 23) app.cfg.dayStartHour = 7;
  if (app.cfg.nightStartHour > 23) app.cfg.nightStartHour = 22;
  if (app.cfg.startPage >= PAGE_COUNT) app.cfg.startPage = 0;
  if (app.cfg.dimmerMode > DIMMER_NIGHT) app.cfg.dimmerMode = DIMMER_AUTO;
  if (app.thr.eco2Alarm < app.thr.eco2Warn) app.thr.eco2Alarm = app.thr.eco2Warn + 100;
  if (app.thr.tvocAlarm < app.thr.tvocWarn) app.thr.tvocAlarm = app.thr.tvocWarn + 100;
  if (app.thr.aqiWarn < 1) app.thr.aqiWarn = 1;
  if (app.thr.aqiWarn > 5) app.thr.aqiWarn = 5;
  if (app.cfg.luxNightThreshold < 0.0f) app.cfg.luxNightThreshold = 8.0f;
  if (app.cfg.luxDayThreshold < app.cfg.luxNightThreshold + 1.0f) app.cfg.luxDayThreshold = app.cfg.luxNightThreshold + 5.0f;
  clearHistory();
}

void saveConfig() {
  prefs.begin("stazamb", false);
  prefs.putBool("autoRot", app.cfg.autoRotateEnabled);
  prefs.putUShort("pageSec", app.cfg.pageIntervalSec);
  prefs.putUShort("sensSec", app.cfg.sensorIntervalSec);
  prefs.putBool("dimEn", app.cfg.dimmerEnabled);
  prefs.putUChar("dimMode", app.cfg.dimmerMode);
  prefs.putUChar("dayHr", app.cfg.dayStartHour);
  prefs.putUChar("nightHr", app.cfg.nightStartHour);
  prefs.putUChar("startPg", app.currentPage);
  prefs.putBool("alertsEn", app.cfg.alertsEnabled);
  prefs.putBool("histEn", app.cfg.historyEnabled);
  prefs.putBool("luxAuto", app.cfg.useLuxForAuto);
  prefs.putFloat("luxNight", app.cfg.luxNightThreshold);
  prefs.putFloat("luxDay", app.cfg.luxDayThreshold);
  prefs.putFloat("tLow", app.thr.tempLow);
  prefs.putFloat("tHigh", app.thr.tempHigh);
  prefs.putFloat("hLow", app.thr.humLow);
  prefs.putFloat("hHigh", app.thr.humHigh);
  prefs.putFloat("pLow", app.thr.pressureLow);
  prefs.putFloat("pHigh", app.thr.pressureHigh);
  prefs.putUShort("eco2W", app.thr.eco2Warn);
  prefs.putUShort("eco2A", app.thr.eco2Alarm);
  prefs.putUShort("tvocW", app.thr.tvocWarn);
  prefs.putUShort("tvocA", app.thr.tvocAlarm);
  prefs.putUChar("aqiW", app.thr.aqiWarn);
  prefs.end();
}

void nextPage() {
  app.currentPage = (app.currentPage + 1) % PAGE_COUNT;
  app.lastPageSwitchMs = millis();
  markDisplayLayoutDirty();
}

void setCurrentPage(uint8_t page) {
  if (page >= PAGE_COUNT) page = PAGE_HOME;
  app.currentPage = page;
  app.lastPageSwitchMs = millis();
  markDisplayLayoutDirty();
}

bool isTimeValid() {
  time_t now = time(nullptr);
  return now > 1700000000;
}

int currentLocalHour() {
  if (!isTimeValid()) return 12;
  time_t now = time(nullptr);
  struct tm ti;
  localtime_r(&now, &ti);
  return ti.tm_hour;
}

bool isNightModeActive() {
  if (!app.cfg.dimmerEnabled) return false;
  if (app.cfg.dimmerMode == DIMMER_DAY) return false;
  if (app.cfg.dimmerMode == DIMMER_NIGHT) return true;

  if (app.cfg.useLuxForAuto && app.status.apdsOk && !isnan(app.data.lux)) {
    if (app.isNightTheme) {
      return app.data.lux < app.cfg.luxDayThreshold;
    }
    return app.data.lux <= app.cfg.luxNightThreshold;
  }

  const int hour = currentLocalHour();
  const int dayStart = app.cfg.dayStartHour;
  const int nightStart = app.cfg.nightStartHour;
  if (dayStart == nightStart) return false;
  if (dayStart < nightStart) return !(hour >= dayStart && hour < nightStart);
  return (hour >= nightStart && hour < dayStart);
}

void markDisplayLayoutDirty() {
  app.layoutDirty = true;
  app.valuesDirty = true;
  app.footerDirty = true;
  app.historyDirty = true;
}
void markDisplayValuesDirty() { app.valuesDirty = true; }
void markDisplayFooterDirty() { app.footerDirty = true; }
void markDisplayHistoryDirty() { app.historyDirty = true; }

void clearHistory() {
  for (uint16_t i = 0; i < HISTORY_SIZE; i++) {
    app.hist.temp[i] = NAN;
    app.hist.hum[i] = NAN;
    app.hist.press[i] = NAN;
    app.hist.eco2[i] = 0;
    app.hist.tvoc[i] = 0;
    app.hist.aqi[i] = 0;
  }
  app.hist.count = 0;
  app.hist.head = 0;
}

void pushHistorySample(const SensorData& d) {
  if (!app.cfg.historyEnabled) return;
  const uint16_t idx = app.hist.head;
  app.hist.temp[idx] = d.temperatureAir;
  app.hist.hum[idx] = d.humidityAir;
  app.hist.press[idx] = d.pressureHpa;
  app.hist.eco2[idx] = d.eco2;
  app.hist.tvoc[idx] = d.tvoc;
  app.hist.aqi[idx] = d.aqi;
  app.hist.head = (app.hist.head + 1) % HISTORY_SIZE;
  if (app.hist.count < HISTORY_SIZE) app.hist.count++;
  markDisplayHistoryDirty();
}

uint16_t historyIndexFromOldest(uint16_t idx) {
  if (app.hist.count == 0) return 0;
  const uint16_t oldest = (app.hist.head + HISTORY_SIZE - app.hist.count) % HISTORY_SIZE;
  return (oldest + idx) % HISTORY_SIZE;
}
