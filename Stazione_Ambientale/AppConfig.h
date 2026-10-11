#pragma once

#include <Arduino.h>

#define WIFI_SSID "INSERISCI_SSID"
#define WIFI_PASS "INSERISCI_PASSWORD"
#define HOSTNAME  "stazione-ambientale"

// Due bus I2C separati
static const uint8_t PIN_I2C0_SDA = 21;  // ENS160 + AHT21
static const uint8_t PIN_I2C0_SCL = 22;
static const uint8_t PIN_I2C1_SDA = 16;  // BMP280 + AHT20 (usiamo solo BMP280)
static const uint8_t PIN_I2C1_SCL = 17;

// Display ST7789 240x320 SPI
static const uint8_t TFT_CS  = 5;
static const uint8_t TFT_DC  = 27;
static const uint8_t TFT_RST = 26;

static const uint32_t WIFI_RETRY_MS        = 15000;
static const uint32_t UI_POLL_MS           = 1000;
static const uint32_t DISPLAY_LOOP_MS      = 60;
static const uint16_t HISTORY_SIZE         = 60;
static const uint16_t DEFAULT_PAGE_SEC     = 10;
static const uint16_t DEFAULT_SENSOR_SEC   = 2;

enum DisplayPage : uint8_t {
  PAGE_HOME = 0,
  PAGE_AIR = 1,
  PAGE_TRENDS = 2,
  PAGE_SYSTEM = 3,
  PAGE_COUNT = 4
};

enum DimmerMode : uint8_t {
  DIMMER_AUTO = 0,
  DIMMER_DAY = 1,
  DIMMER_NIGHT = 2
};

struct SensorData {
  float pressureHpa = NAN;      // BMP280
  float temperatureAir = NAN;   // AHT21
  float humidityAir = NAN;      // AHT21
  float lux = NAN;              // APDS-9930 luminosita ambiente
  uint8_t aqi = 0;              // ENS160
  uint16_t tvoc = 0;            // ENS160
  uint16_t eco2 = 0;            // ENS160
  uint8_t ensStatus = 0;        // ENS160
};

struct SensorState {
  bool bmpOk = false;
  bool ahtOk = false;
  bool ensOk = false;
  bool apdsOk = false;
  bool wifiOk = false;
  bool ntpOk = false;
};

struct RuntimeConfig {
  bool autoRotateEnabled = true;
  uint16_t pageIntervalSec = DEFAULT_PAGE_SEC;
  uint16_t sensorIntervalSec = DEFAULT_SENSOR_SEC;
  bool dimmerEnabled = true;
  uint8_t dimmerMode = DIMMER_AUTO;
  uint8_t dayStartHour = 7;
  uint8_t nightStartHour = 22;
  uint8_t startPage = 0;
  bool alertsEnabled = true;
  bool historyEnabled = true;
  bool useLuxForAuto = true;
  float luxNightThreshold = 8.0f;
  float luxDayThreshold = 20.0f;
};

struct ThresholdConfig {
  float tempLow = 18.0f;
  float tempHigh = 27.0f;
  float humLow = 35.0f;
  float humHigh = 65.0f;
  float pressureLow = 995.0f;
  float pressureHigh = 1025.0f;
  uint16_t eco2Warn = 900;
  uint16_t eco2Alarm = 1400;
  uint16_t tvocWarn = 350;
  uint16_t tvocAlarm = 1200;
  uint8_t aqiWarn = 3;
};

struct AlertState {
  bool tempLow = false;
  bool tempHigh = false;
  bool humLow = false;
  bool humHigh = false;
  bool pressureLow = false;
  bool pressureHigh = false;
  bool eco2Warn = false;
  bool eco2Alarm = false;
  bool tvocWarn = false;
  bool tvocAlarm = false;
  bool aqiWarn = false;
  bool any = false;
  uint8_t count = 0;
  String summary;
};

struct HistoryBuffer {
  float temp[HISTORY_SIZE];
  float hum[HISTORY_SIZE];
  float press[HISTORY_SIZE];
  uint16_t eco2[HISTORY_SIZE];
  uint16_t tvoc[HISTORY_SIZE];
  uint8_t aqi[HISTORY_SIZE];
  uint16_t count = 0;
  uint16_t head = 0;
};

struct AppState {
  SensorData data;
  SensorState status;
  RuntimeConfig cfg;
  ThresholdConfig thr;
  AlertState alerts;
  HistoryBuffer hist;

  uint8_t currentPage = PAGE_HOME;
  uint32_t lastSensorReadMs = 0;
  uint32_t lastPageSwitchMs = 0;
  uint32_t lastWiFiAttemptMs = 0;
  uint32_t lastUiTickMs = 0;
  uint32_t lastDisplayLoopMs = 0;

  bool layoutDirty = true;
  bool valuesDirty = true;
  bool footerDirty = true;
  bool historyDirty = true;
  bool isNightTheme = false;
};

extern AppState app;

String formatUptime(uint32_t ms);
String aqiText(uint8_t aqi);
String ensStatusText(uint8_t st);
String boolTxt(bool v);
String currentPageName();
String sensorStateText();
String luxStateText();
String dimmerModeText(uint8_t mode);
String alertSummary();

void loadConfig();
void saveConfig();
void nextPage();
void setCurrentPage(uint8_t page);
bool isTimeValid();
int currentLocalHour();
bool isNightModeActive();
void markDisplayLayoutDirty();
void markDisplayValuesDirty();
void markDisplayFooterDirty();
void markDisplayHistoryDirty();
void clearHistory();
void pushHistorySample(const SensorData& data);
uint16_t historyIndexFromOldest(uint16_t idx);
