#include "AppConfig.h"
#include "SensorsManager.h"
#include "DisplayUI.h"
#include "WebUI.h"

void setup() {
  Serial.begin(115200);
  delay(250);

  loadConfig();
  app.currentPage = app.cfg.startPage;
  app.lastPageSwitchMs = millis();

  beginDisplay();
  initI2CBuses();
  scanI2CBuses();
  beginSensors();
  connectWiFi();
  beginWeb();

  readSensors();
  markDisplayLayoutDirty();
  drawDisplay();
}

void loop() {
  handleWeb();
  ensureWiFi();

  const uint32_t now = millis();
  if (now - app.lastSensorReadMs >= (uint32_t)app.cfg.sensorIntervalSec * 1000UL) {
    app.lastSensorReadMs = now;
    readSensors();
  }

  if (app.cfg.autoRotateEnabled && (now - app.lastPageSwitchMs >= (uint32_t)app.cfg.pageIntervalSec * 1000UL)) {
    nextPage();
  }

  if (now - app.lastUiTickMs >= UI_POLL_MS) {
    app.lastUiTickMs = now;
    const bool nightNow = isNightModeActive();
    if (nightNow != app.isNightTheme) markDisplayLayoutDirty();
    else markDisplayFooterDirty();
  }

  if (now - app.lastDisplayLoopMs >= DISPLAY_LOOP_MS) {
    app.lastDisplayLoopMs = now;
    drawDisplay();
  }
}
