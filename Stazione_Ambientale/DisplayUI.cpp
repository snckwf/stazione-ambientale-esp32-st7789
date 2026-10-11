#include "DisplayUI.h"

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <WiFi.h>
#include <math.h>

static Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

struct Theme {
  uint16_t bg;
  uint16_t header;
  uint16_t card;
  uint16_t border;
  uint16_t label;
  uint16_t text;
  uint16_t muted;
  uint16_t accent;
  uint16_t warn;
  uint16_t bad;
  uint16_t good;
};

static Theme dayTheme = { ST77XX_BLACK, ST77XX_BLUE, 0x10A2, 0x31A6, ST77XX_CYAN, ST77XX_WHITE, 0x94B2, ST77XX_CYAN, ST77XX_YELLOW, ST77XX_RED, ST77XX_GREEN };
static Theme nightTheme = { ST77XX_BLACK, 0x0140, 0x0841, 0x18C3, 0x03E0, 0x9CF3, 0x4A49, 0x03E0, 0xA145, 0x8000, 0x03E0 };

struct Box { int x, y, w, h; };
static const Box cards[4] = { {10,48,145,76}, {165,48,145,76}, {10,132,145,76}, {165,132,145,76} };
static const uint16_t DARK_GREY = 0x4208;
static uint8_t renderedPage = 255;
static bool renderedNight = false;
static String lastV1, lastV2, lastV3, lastV4, lastFooter;

static Theme& theme() { return app.isNightTheme ? nightTheme : dayTheme; }
static uint16_t rgb565(uint8_t r,uint8_t g,uint8_t b){ return ((r & 0xF8)<<8)|((g & 0xFC)<<3)|(b>>3); }

static uint16_t aqiColor(uint8_t aqi) {
  Theme& th = theme();
  switch (aqi) {
    case 1: return th.good;
    case 2: return th.accent;
    case 3: return th.warn;
    case 4: return rgb565(255,140,0);
    case 5: return th.bad;
    default: return th.text;
  }
}
static uint16_t tempColor(float t) {
  Theme& th = theme();
  if (isnan(t)) return th.text;
  if (t < app.thr.tempLow) return th.accent;
  if (t > app.thr.tempHigh) return th.bad;
  return th.good;
}
static uint16_t humColor(float h) {
  Theme& th = theme();
  if (isnan(h)) return th.text;
  if (h < app.thr.humLow || h > app.thr.humHigh) return th.warn;
  return th.good;
}
static uint16_t pressureColor(float p) {
  Theme& th = theme();
  if (isnan(p)) return th.text;
  if (p < app.thr.pressureLow) return th.accent;
  if (p > app.thr.pressureHigh) return th.warn;
  return th.good;
}
static uint16_t eco2Color(uint16_t eco2) {
  Theme& th = theme();
  if (eco2 >= app.thr.eco2Alarm) return th.bad;
  if (eco2 >= app.thr.eco2Warn) return th.warn;
  return th.good;
}
static uint16_t tvocColor(uint16_t tvoc) {
  Theme& th = theme();
  if (tvoc >= app.thr.tvocAlarm) return th.bad;
  if (tvoc >= app.thr.tvocWarn) return th.warn;
  return th.good;
}

void beginDisplay() {
  tft.init(240, 320);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);
}

static void drawHeader() {
  Theme& th = theme();
  tft.fillRect(0, 0, 320, 34, th.header);
  tft.setTextColor(th.text);
  tft.setTextSize(2);
  tft.setCursor(10, 8);
  tft.print("Stazione Ambientale");
}

static void clearValueBox(const Box& b) {
  Theme& th = theme();
  tft.fillRect(b.x + 6, b.y + 22, b.w - 12, b.h - 28, th.card);
}

static void drawCardFrame(const Box& b, const String& label) {
  Theme& th = theme();
  tft.drawRoundRect(b.x, b.y, b.w, b.h, 10, th.border);
  tft.setTextColor(th.label);
  tft.setTextSize(2);
  tft.setCursor(b.x + 8, b.y + 6);
  tft.print(label);
}

static void drawFooter() {
  Theme& th = theme();
  String footer = currentPageName() + " | " + (app.isNightTheme ? "Notte" : "Giorno") + " | Allarmi " + String(app.alerts.count);
  if (footer == lastFooter && !app.footerDirty) return;
  lastFooter = footer;
  tft.fillRect(0, 216, 320, 24, th.bg);
  tft.drawLine(0, 216, 319, 216, th.border);
  tft.setTextColor(th.muted);
  tft.setTextSize(1);
  tft.setCursor(8, 223);
  tft.print(footer);
  tft.setCursor(205, 223);
  tft.print(WiFi.isConnected() ? WiFi.localIP().toString() : String("offline"));
  app.footerDirty = false;
}

static void drawPageLayout() {
  Theme& th = theme();
  tft.fillScreen(th.bg);
  drawHeader();
  if (app.currentPage == PAGE_HOME) {
    drawCardFrame(cards[0], "Temp.");
    drawCardFrame(cards[1], "Umid.");
    drawCardFrame(cards[2], "Press.");
    drawCardFrame(cards[3], "Qualita");
  } else if (app.currentPage == PAGE_AIR) {
    drawCardFrame(cards[0], "AQI");
    drawCardFrame(cards[1], "TVOC");
    drawCardFrame(cards[2], "eCO2");
    drawCardFrame(cards[3], "Allarmi");
  } else if (app.currentPage == PAGE_SYSTEM) {
    drawCardFrame(cards[0], "IP");
    drawCardFrame(cards[1], "Lux");
    drawCardFrame(cards[2], "Uptime");
    drawCardFrame(cards[3], "Dimmer");
  } else if (app.currentPage == PAGE_TRENDS) {
    tft.drawRoundRect(8, 46, 304, 48, 8, th.border);
    tft.drawRoundRect(8, 100, 304, 48, 8, th.border);
    tft.drawRoundRect(8, 154, 304, 48, 8, th.border);
    tft.setTextColor(th.label); tft.setTextSize(2);
    tft.setCursor(14, 52); tft.print("Temp");
    tft.setCursor(14, 106); tft.print("Umid");
    tft.setCursor(14, 160); tft.print("Press");
  }
  lastV1 = lastV2 = lastV3 = lastV4 = "";
  app.layoutDirty = false;
  app.valuesDirty = true;
  app.footerDirty = true;
  app.historyDirty = true;
  renderedPage = app.currentPage;
  renderedNight = app.isNightTheme;
}

static void drawValue(const Box& b, const String& value, uint16_t color, uint8_t size) {
  clearValueBox(b);
  tft.setTextColor(color);
  tft.setTextSize(size);
  int16_t x1,y1; uint16_t w,h;
  tft.getTextBounds(value,0,0,&x1,&y1,&w,&h);
  int x = b.x + ((b.w - (int)w)/2); if (x < b.x+6) x = b.x+6;
  int y = b.y + 38;
  tft.setCursor(x,y);
  tft.print(value);
}

static void updateCardPages() {
  if (!app.valuesDirty) return;
  Theme& th = theme();
  String v1, v2, v3, v4; uint16_t c1 = th.text, c2 = th.text, c3 = th.text, c4 = th.text; uint8_t s1=3,s2=3,s3=2,s4=2;
  if (app.currentPage == PAGE_HOME) {
    v1 = isnan(app.data.temperatureAir) ? "--" : String(app.data.temperatureAir,1) + "C";
    v2 = isnan(app.data.humidityAir) ? "--" : String(app.data.humidityAir,0) + "%";
    v3 = isnan(app.data.pressureHpa) ? "--" : String(app.data.pressureHpa,0) + "hPa";
    v4 = aqiText(app.data.aqi);
    c1 = tempColor(app.data.temperatureAir); c2 = humColor(app.data.humidityAir); c3 = pressureColor(app.data.pressureHpa); c4 = aqiColor(app.data.aqi);
  } else if (app.currentPage == PAGE_AIR) {
    v1 = String(app.data.aqi);
    v2 = String(app.data.tvoc) + "ppb";
    v3 = String(app.data.eco2) + "ppm";
    v4 = app.alerts.any ? String(app.alerts.count) : String("0");
    c1 = aqiColor(app.data.aqi); c2 = tvocColor(app.data.tvoc); c3 = eco2Color(app.data.eco2); c4 = app.alerts.any ? th.warn : th.good;
    s1 = 5; s4 = 5;
  } else if (app.currentPage == PAGE_SYSTEM) {
    v1 = WiFi.isConnected() ? WiFi.localIP().toString() : String("offline");
    v2 = isnan(app.data.lux) ? String("--") : String((double)app.data.lux, 0) + "lx";
    v3 = formatUptime(millis());
    v4 = app.status.apdsOk ? dimmerModeText(app.cfg.dimmerMode) : String("No APDS");
    s1 = 2; s2 = 2; s3 = 1; s4 = 2;
  }
  if (app.currentPage != PAGE_TRENDS) {
    if (v1 != lastV1) { drawValue(cards[0], v1, c1, s1); lastV1 = v1; }
    if (v2 != lastV2) { drawValue(cards[1], v2, c2, s2); lastV2 = v2; }
    if (v3 != lastV3) { drawValue(cards[2], v3, c3, s3); lastV3 = v3; }
    if (v4 != lastV4) { drawValue(cards[3], v4, c4, s4); lastV4 = v4; }
    app.valuesDirty = false;
  }
}

static void drawMiniGraph(int x, int y, int w, int h, const float* arr, uint16_t count, uint16_t color, uint8_t decimals) {
  Theme& th = theme();
  tft.fillRect(x, y, w, h, th.bg);
  tft.drawRect(x, y, w, h, th.border);
  if (count < 2) return;
  float minV = 999999.0f, maxV = -999999.0f;
  for (uint16_t i = 0; i < count; i++) {
    float v = arr[historyIndexFromOldest(i)];
    if (isnan(v)) continue;
    if (v < minV) minV = v;
    if (v > maxV) maxV = v;
  }
  if (minV > maxV) return;
  if (fabsf(maxV - minV) < 0.5f) { maxV += 0.25f; minV -= 0.25f; }
  tft.setTextColor(th.muted); tft.setTextSize(1);
  tft.setCursor(x + w - 40, y + 2); tft.print(String((double)maxV, (unsigned int)decimals));
  tft.setCursor(x + w - 40, y + h - 10); tft.print(String((double)minV, (unsigned int)decimals));
  for (uint16_t i = 1; i < count; i++) {
    float va = arr[historyIndexFromOldest(i - 1)];
    float vb = arr[historyIndexFromOldest(i)];
    if (isnan(va) || isnan(vb)) continue;
    int x1 = x + 2 + ((w - 45) * (int)(i - 1)) / (count - 1);
    int x2 = x + 2 + ((w - 45) * (int)i) / (count - 1);
    int y1 = y + h - 3 - (int)((va - minV) * (float)(h - 8) / (maxV - minV));
    int y2 = y + h - 3 - (int)((vb - minV) * (float)(h - 8) / (maxV - minV));
    tft.drawLine(x1, y1, x2, y2, color);
  }
}

static void drawTrendsPage() {
  if (!app.historyDirty && !app.valuesDirty) return;
  drawMiniGraph(60, 50, 246, 40, app.hist.temp, app.hist.count, tempColor(app.data.temperatureAir), 1);
  drawMiniGraph(60, 104, 246, 40, app.hist.hum, app.hist.count, humColor(app.data.humidityAir), 0);
  drawMiniGraph(60, 158, 246, 40, app.hist.press, app.hist.count, pressureColor(app.data.pressureHpa), 0);
  app.historyDirty = false;
  app.valuesDirty = false;
}

void drawDisplay() {
  app.isNightTheme = isNightModeActive();
  if (renderedPage != app.currentPage || renderedNight != app.isNightTheme || app.layoutDirty) {
    drawPageLayout();
  }
  if (app.currentPage == PAGE_TRENDS) drawTrendsPage();
  else updateCardPages();
  drawFooter();
}
