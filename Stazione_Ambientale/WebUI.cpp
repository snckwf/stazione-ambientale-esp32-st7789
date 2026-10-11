#include "WebUI.h"

#include <WiFi.h>
#include <WebServer.h>
#include <time.h>

static WebServer server(80);

static void initTimeIfNeeded() {
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

void connectWiFi() {
  if (WiFi.isConnected()) {
    app.status.wifiOk = true;
    app.status.ntpOk = isTimeValid();
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[WiFi] connessione");
  const uint32_t start = millis();
  while (!WiFi.isConnected() && millis() - start < 12000) {
    delay(300);
    Serial.print('.');
  }
  Serial.println();
  app.status.wifiOk = WiFi.isConnected();
  if (app.status.wifiOk) {
    Serial.print("[WiFi] OK IP: "); Serial.println(WiFi.localIP());
    initTimeIfNeeded();
    app.status.ntpOk = isTimeValid();
    markDisplayFooterDirty();
  } else {
    Serial.println("[WiFi] non connesso");
  }
}

void ensureWiFi() {
  if (WiFi.isConnected()) {
    app.status.wifiOk = true;
    app.status.ntpOk = isTimeValid();
    return;
  }
  app.status.wifiOk = false;
  app.status.ntpOk = false;
  if (millis() - app.lastWiFiAttemptMs >= WIFI_RETRY_MS) {
    app.lastWiFiAttemptMs = millis();
    connectWiFi();
  }
}

static String jsonData() {
  String s = "{";
  s += "\"pressureHpa\":" + (isnan(app.data.pressureHpa) ? String("null") : String(app.data.pressureHpa, 1)) + ",";
  s += "\"temperatureAir\":" + (isnan(app.data.temperatureAir) ? String("null") : String(app.data.temperatureAir, 1)) + ",";
  s += "\"humidityAir\":" + (isnan(app.data.humidityAir) ? String("null") : String(app.data.humidityAir, 1)) + ",";
  s += "\"lux\":" + (isnan(app.data.lux) ? String("null") : String((double)app.data.lux, 0)) + ",";
  s += "\"aqi\":" + String(app.data.aqi) + ",";
  s += "\"tvoc\":" + String(app.data.tvoc) + ",";
  s += "\"eco2\":" + String(app.data.eco2) + ",";
  s += "\"ensStatus\":\"" + ensStatusText(app.data.ensStatus) + "\",";
  s += "\"aqiText\":\"" + aqiText(app.data.aqi) + "\",";
  s += "\"wifiConnected\":" + String(app.status.wifiOk ? "true" : "false") + ",";
  s += "\"ntpOk\":" + String(app.status.ntpOk ? "true" : "false") + ",";
  s += "\"ip\":\"" + (WiFi.isConnected() ? WiFi.localIP().toString() : String("offline")) + "\",";
  s += "\"rssi\":" + String(WiFi.isConnected() ? WiFi.RSSI() : 0) + ",";
  s += "\"uptime\":\"" + formatUptime(millis()) + "\",";
  s += "\"page\":" + String(app.currentPage) + ",";
  s += "\"pageName\":\"" + currentPageName() + "\",";
  s += "\"isNightTheme\":" + String(app.isNightTheme ? "true" : "false") + ",";
  s += "\"dimmerModeText\":\"" + dimmerModeText(app.cfg.dimmerMode) + "\",";
  s += "\"sensorState\":\"" + sensorStateText() + "\",";
  s += "\"luxState\":\"" + luxStateText() + "\",";
  s += "\"apdsOk\":" + String(app.status.apdsOk ? "true" : "false") + ",";
  s += "\"alertCount\":" + String(app.alerts.count) + ",";
  s += "\"alertAny\":" + String(app.alerts.any ? "true" : "false") + ",";
  s += "\"alertSummary\":\"" + alertSummary() + "\"";
  s += "}";
  return s;
}

static String jsonConfig() {
  String s = "{";
  s += "\"autoRotateEnabled\":" + String(app.cfg.autoRotateEnabled ? "true" : "false") + ",";
  s += "\"pageIntervalSec\":" + String(app.cfg.pageIntervalSec) + ",";
  s += "\"sensorIntervalSec\":" + String(app.cfg.sensorIntervalSec) + ",";
  s += "\"dimmerEnabled\":" + String(app.cfg.dimmerEnabled ? "true" : "false") + ",";
  s += "\"dimmerMode\":" + String(app.cfg.dimmerMode) + ",";
  s += "\"dayStartHour\":" + String(app.cfg.dayStartHour) + ",";
  s += "\"nightStartHour\":" + String(app.cfg.nightStartHour) + ",";
  s += "\"alertsEnabled\":" + String(app.cfg.alertsEnabled ? "true" : "false") + ",";
  s += "\"historyEnabled\":" + String(app.cfg.historyEnabled ? "true" : "false") + ",";
  s += "\"useLuxForAuto\":" + String(app.cfg.useLuxForAuto ? "true" : "false") + ",";
  s += "\"luxNightThreshold\":" + String((double)app.cfg.luxNightThreshold, 1) + ",";
  s += "\"luxDayThreshold\":" + String((double)app.cfg.luxDayThreshold, 1) + ",";
  s += "\"tempLow\":" + String(app.thr.tempLow, 1) + ",";
  s += "\"tempHigh\":" + String(app.thr.tempHigh, 1) + ",";
  s += "\"humLow\":" + String(app.thr.humLow, 1) + ",";
  s += "\"humHigh\":" + String(app.thr.humHigh, 1) + ",";
  s += "\"pressureLow\":" + String(app.thr.pressureLow, 1) + ",";
  s += "\"pressureHigh\":" + String(app.thr.pressureHigh, 1) + ",";
  s += "\"eco2Warn\":" + String(app.thr.eco2Warn) + ",";
  s += "\"eco2Alarm\":" + String(app.thr.eco2Alarm) + ",";
  s += "\"tvocWarn\":" + String(app.thr.tvocWarn) + ",";
  s += "\"tvocAlarm\":" + String(app.thr.tvocAlarm) + ",";
  s += "\"aqiWarn\":" + String(app.thr.aqiWarn);
  s += "}";
  return s;
}

static String jsonAlerts() {
  String s = "{";
  s += "\"any\":" + String(app.alerts.any ? "true" : "false") + ",";
  s += "\"count\":" + String(app.alerts.count) + ",";
  s += "\"summary\":\"" + alertSummary() + "\",";
  s += "\"tempLow\":" + String(app.alerts.tempLow ? "true" : "false") + ",";
  s += "\"tempHigh\":" + String(app.alerts.tempHigh ? "true" : "false") + ",";
  s += "\"humLow\":" + String(app.alerts.humLow ? "true" : "false") + ",";
  s += "\"humHigh\":" + String(app.alerts.humHigh ? "true" : "false") + ",";
  s += "\"pressureLow\":" + String(app.alerts.pressureLow ? "true" : "false") + ",";
  s += "\"pressureHigh\":" + String(app.alerts.pressureHigh ? "true" : "false") + ",";
  s += "\"eco2Warn\":" + String(app.alerts.eco2Warn ? "true" : "false") + ",";
  s += "\"eco2Alarm\":" + String(app.alerts.eco2Alarm ? "true" : "false") + ",";
  s += "\"tvocWarn\":" + String(app.alerts.tvocWarn ? "true" : "false") + ",";
  s += "\"tvocAlarm\":" + String(app.alerts.tvocAlarm ? "true" : "false") + ",";
  s += "\"aqiWarn\":" + String(app.alerts.aqiWarn ? "true" : "false");
  s += "}";
  return s;
}

static String jsonHistory() {
  String s = "{";
  s += "\"count\":" + String(app.hist.count) + ",";
  s += "\"temp\":[";
  for (uint16_t i = 0; i < app.hist.count; i++) {
    if (i) s += ",";
    float v = app.hist.temp[historyIndexFromOldest(i)];
    s += isnan(v) ? String("null") : String(v, 1);
  }
  s += "],\"hum\":[";
  for (uint16_t i = 0; i < app.hist.count; i++) {
    if (i) s += ",";
    float v = app.hist.hum[historyIndexFromOldest(i)];
    s += isnan(v) ? String("null") : String(v, 1);
  }
  s += "],\"press\":[";
  for (uint16_t i = 0; i < app.hist.count; i++) {
    if (i) s += ",";
    float v = app.hist.press[historyIndexFromOldest(i)];
    s += isnan(v) ? String("null") : String(v, 1);
  }
  s += "],\"eco2\":[";
  for (uint16_t i = 0; i < app.hist.count; i++) { if (i) s += ","; s += String(app.hist.eco2[historyIndexFromOldest(i)]); }
  s += "],\"tvoc\":[";
  for (uint16_t i = 0; i < app.hist.count; i++) { if (i) s += ","; s += String(app.hist.tvoc[historyIndexFromOldest(i)]); }
  s += "],\"aqi\":[";
  for (uint16_t i = 0; i < app.hist.count; i++) { if (i) s += ","; s += String(app.hist.aqi[historyIndexFromOldest(i)]); }
  s += "]}";
  return s;
}

static String csvHistory() {
  String s = "index,temp,humidity,pressure,eco2,tvoc,aqi\n";
  for (uint16_t i = 0; i < app.hist.count; i++) {
    const uint16_t idx = historyIndexFromOldest(i);
    s += String(i) + ",";
    s += (isnan(app.hist.temp[idx]) ? String("") : String(app.hist.temp[idx], 1)) + ",";
    s += (isnan(app.hist.hum[idx]) ? String("") : String(app.hist.hum[idx], 1)) + ",";
    s += (isnan(app.hist.press[idx]) ? String("") : String(app.hist.press[idx], 1)) + ",";
    s += String(app.hist.eco2[idx]) + "," + String(app.hist.tvoc[idx]) + "," + String(app.hist.aqi[idx]) + "\n";
  }
  return s;
}

static String haYaml() {
  const String host = WiFi.isConnected() ? ("http://" + WiFi.localIP().toString()) : String("http://IP_ESP32");
  String s;
  s += "rest:\n";
  s += "  - resource: " + host + "/api/data\n";
  s += "    scan_interval: 10\n";
  s += "    sensor:\n";
  s += "      - name: Stazione Temperatura\n        value_template: \"{{ value_json.temperatureAir }}\"\n        unit_of_measurement: \"°C\"\n";
  s += "      - name: Stazione Umidita\n        value_template: \"{{ value_json.humidityAir }}\"\n        unit_of_measurement: \"%\"\n";
  s += "      - name: Stazione Pressione\n        value_template: \"{{ value_json.pressureHpa }}\"\n        unit_of_measurement: \"hPa\"\n";
  s += "      - name: Stazione Lux\n        value_template: \"{{ value_json.lux }}\"\n        unit_of_measurement: \"lx\"\n";
  s += "      - name: Stazione AQI\n        value_template: \"{{ value_json.aqi }}\"\n";
  s += "      - name: Stazione TVOC\n        value_template: \"{{ value_json.tvoc }}\"\n        unit_of_measurement: \"ppb\"\n";
  s += "      - name: Stazione eCO2\n        value_template: \"{{ value_json.eco2 }}\"\n        unit_of_measurement: \"ppm\"\n";
  s += "      - name: Stazione Allarmi\n        value_template: \"{{ value_json.alertCount }}\"\n";
  s += "\n# Usa /api/alerts per automazioni e notifiche personalizzate\n";
  return s;
}

static void handleRoot();
static void handleApiData() { server.send(200, "application/json", jsonData()); }
static void handleApiConfigGet() { server.send(200, "application/json", jsonConfig()); }
static void handleApiAlerts() { server.send(200, "application/json", jsonAlerts()); }
static void handleApiHistory() { server.send(200, "application/json", jsonHistory()); }
static void handleApiExportCsv() { server.send(200, "text/csv", csvHistory()); }
static void handleApiHaYaml() { server.send(200, "text/plain; charset=utf-8", haYaml()); }

static void handleApiConfigPost() {
  if (server.hasArg("autoRotateEnabled")) app.cfg.autoRotateEnabled = server.arg("autoRotateEnabled").toInt() != 0;
  if (server.hasArg("pageIntervalSec")) app.cfg.pageIntervalSec = constrain(server.arg("pageIntervalSec").toInt(), 3, 300);
  if (server.hasArg("sensorIntervalSec")) app.cfg.sensorIntervalSec = constrain(server.arg("sensorIntervalSec").toInt(), 1, 60);
  if (server.hasArg("dimmerEnabled")) app.cfg.dimmerEnabled = server.arg("dimmerEnabled").toInt() != 0;
  if (server.hasArg("dimmerMode")) app.cfg.dimmerMode = constrain(server.arg("dimmerMode").toInt(), 0, 2);
  if (server.hasArg("dayStartHour")) app.cfg.dayStartHour = constrain(server.arg("dayStartHour").toInt(), 0, 23);
  if (server.hasArg("nightStartHour")) app.cfg.nightStartHour = constrain(server.arg("nightStartHour").toInt(), 0, 23);
  if (server.hasArg("alertsEnabled")) app.cfg.alertsEnabled = server.arg("alertsEnabled").toInt() != 0;
  if (server.hasArg("historyEnabled")) app.cfg.historyEnabled = server.arg("historyEnabled").toInt() != 0;
  if (server.hasArg("useLuxForAuto")) app.cfg.useLuxForAuto = server.arg("useLuxForAuto").toInt() != 0;
  if (server.hasArg("luxNightThreshold")) app.cfg.luxNightThreshold = max(0.0f, server.arg("luxNightThreshold").toFloat());
  if (server.hasArg("luxDayThreshold")) app.cfg.luxDayThreshold = max(app.cfg.luxNightThreshold + 1.0f, server.arg("luxDayThreshold").toFloat());
  if (server.hasArg("tempLow")) app.thr.tempLow = server.arg("tempLow").toFloat();
  if (server.hasArg("tempHigh")) app.thr.tempHigh = server.arg("tempHigh").toFloat();
  if (server.hasArg("humLow")) app.thr.humLow = server.arg("humLow").toFloat();
  if (server.hasArg("humHigh")) app.thr.humHigh = server.arg("humHigh").toFloat();
  if (server.hasArg("pressureLow")) app.thr.pressureLow = server.arg("pressureLow").toFloat();
  if (server.hasArg("pressureHigh")) app.thr.pressureHigh = server.arg("pressureHigh").toFloat();
  if (server.hasArg("eco2Warn")) app.thr.eco2Warn = constrain(server.arg("eco2Warn").toInt(), 400, 5000);
  if (server.hasArg("eco2Alarm")) app.thr.eco2Alarm = constrain(server.arg("eco2Alarm").toInt(), 500, 8000);
  if (server.hasArg("tvocWarn")) app.thr.tvocWarn = constrain(server.arg("tvocWarn").toInt(), 50, 5000);
  if (server.hasArg("tvocAlarm")) app.thr.tvocAlarm = constrain(server.arg("tvocAlarm").toInt(), 100, 10000);
  if (server.hasArg("aqiWarn")) app.thr.aqiWarn = constrain(server.arg("aqiWarn").toInt(), 1, 5);
  if (app.thr.eco2Alarm < app.thr.eco2Warn) app.thr.eco2Alarm = app.thr.eco2Warn + 100;
  if (app.thr.tvocAlarm < app.thr.tvocWarn) app.thr.tvocAlarm = app.thr.tvocWarn + 100;
  saveConfig();
  markDisplayLayoutDirty();
  server.send(200, "text/plain", "OK");
}

static void handleApiPagePost() {
  if (server.hasArg("page")) {
    setCurrentPage((uint8_t)constrain(server.arg("page").toInt(), 0, PAGE_COUNT - 1));
    saveConfig();
  }
  server.send(200, "text/plain", "OK");
}

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html><html lang="it"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Stazione Ambientale</title>
<style>
:root{--bg:#0b1020;--card:#151d36;--card2:#1c2748;--line:#2b3765;--text:#eef3ff;--muted:#a6b3d1;--accent:#68b3ff;--ok:#2ecc71;--warn:#f1c40f;--bad:#e74c3c}*{box-sizing:border-box}body{margin:0;font-family:Arial,Helvetica,sans-serif;background:linear-gradient(180deg,#08101d,#0d1631);color:var(--text)}.wrap{max-width:1180px;margin:0 auto;padding:18px}.top{display:flex;justify-content:space-between;gap:12px;flex-wrap:wrap;margin-bottom:14px}.title{font-size:26px;font-weight:700}.sub{font-size:14px;color:var(--muted)}.chips{display:flex;gap:8px;flex-wrap:wrap}.chip{padding:8px 12px;border-radius:999px;background:var(--card);border:1px solid var(--line);font-size:13px}.tabs{display:flex;gap:8px;flex-wrap:wrap;margin:16px 0}.tabs button,.actions button,.saveBtn{padding:10px 14px;border-radius:12px;border:1px solid var(--line);background:var(--card);color:var(--text);cursor:pointer}.tabs button.active,.saveBtn{background:var(--accent);color:#07111f;border-color:var(--accent);font-weight:700}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:14px}.card{background:linear-gradient(180deg,var(--card),var(--card2));border:1px solid var(--line);border-radius:18px;padding:16px;box-shadow:0 10px 24px rgba(0,0,0,.22)}.label{font-size:13px;color:var(--muted);margin-bottom:6px}.value{font-size:34px;font-weight:700;line-height:1.1}.unit{font-size:16px;color:var(--muted);margin-left:6px}.small{font-size:13px;color:var(--muted);margin-top:8px}.page{display:none}.page.active{display:block}.panel{margin-top:18px}.form{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px}.form2{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:12px}.row{display:flex;gap:8px;align-items:center}.switch{transform:scale(1.2)}label{display:block;font-size:13px;color:var(--muted);margin-bottom:6px}input,select,textarea{width:100%;padding:10px 12px;border-radius:12px;border:1px solid var(--line);background:#0f1730;color:var(--text)}textarea{min-height:180px;resize:vertical}.ok{color:var(--ok)}.warn{color:var(--warn)}.bad{color:var(--bad)}a{color:#8ec7ff}
</style></head><body><div class="wrap">
<div class="top"><div><div class="title">Stazione Ambientale</div><div class="sub">Storico dati · mini grafici · Home Assistant · soglie personalizzate · export rete · APDS-9930 luce</div></div><div class="chips"><div class="chip" id="wifiChip">Wi‑Fi: ...</div><div class="chip" id="themeChip">Tema: ...</div><div class="chip" id="alertChip">Allarmi: ...</div></div></div>
<div class="tabs"><button class="tab active" data-page="dati">Dati</button><button class="tab" data-page="allarmi">Allarmi</button><button class="tab" data-page="config">Configurazione</button><button class="tab" data-page="ha">Home Assistant / Export</button></div>
<section class="page active" id="page-dati"><div class="grid">
<div class="card"><div class="label">Temperatura</div><div class="value"><span id="tempAir">--</span><span class="unit">°C</span></div><div class="small">AHT21</div></div>
<div class="card"><div class="label">Umidità</div><div class="value"><span id="humAir">--</span><span class="unit">%</span></div><div class="small">AHT21</div></div>
<div class="card"><div class="label">Pressione</div><div class="value"><span id="pressBmp">--</span><span class="unit">hPa</span></div><div class="small">BMP280</div></div>
<div class="card"><div class="label">Qualità aria</div><div class="value"><span id="aqiText">--</span></div><div class="small">AQI / TVOC / eCO2</div></div>
<div class="card"><div class="label">AQI</div><div class="value"><span id="aqi">--</span></div></div>
<div class="card"><div class="label">TVOC</div><div class="value"><span id="tvoc">--</span><span class="unit">ppb</span></div></div>
<div class="card"><div class="label">eCO2</div><div class="value"><span id="eco2">--</span><span class="unit">ppm</span></div></div>
<div class="card"><div class="label">Luce ambiente</div><div class="value"><span id="lux">--</span><span class="unit">lx</span></div><div class="small">APDS-9930</div></div>
<div class="card"><div class="label">Storico campioni</div><div class="value"><span id="histCount">--</span></div><div class="small">buffer circolare</div></div>
</div></section>
<section class="page" id="page-allarmi"><div class="grid">
<div class="card"><div class="label">Stato allarmi</div><div class="value" style="font-size:24px"><span id="alertSummary">--</span></div><div class="small">soglie personalizzate</div></div>
<div class="card"><div class="label">Conta allarmi</div><div class="value"><span id="alertCount">0</span></div></div>
<div class="card"><div class="label">Pagina display</div><div class="value" style="font-size:24px"><span id="pageName">--</span></div></div>
<div class="card"><div class="label">Uptime</div><div class="value" style="font-size:24px"><span id="uptime">--</span></div></div>
</div></section>
<section class="page" id="page-config"><div class="card panel"><div class="form">
<div><label><input class="switch" id="autoRotateEnabled" type="checkbox"> Rotazione automatica pagine</label></div>
<div><label for="pageIntervalSec">Intervallo cambio pagina (s)</label><input id="pageIntervalSec" type="number" min="3" max="300"></div>
<div><label for="sensorIntervalSec">Intervallo lettura sensori (s)</label><input id="sensorIntervalSec" type="number" min="1" max="60"></div>
<div><label><input class="switch" id="dimmerEnabled" type="checkbox"> Dimmer software attivo</label></div>
<div><label for="dimmerMode">Modo dimmer</label><select id="dimmerMode"><option value="0">Auto</option><option value="1">Sempre giorno</option><option value="2">Sempre notte</option></select></div>
<div><label for="dayStartHour">Ora inizio giorno</label><input id="dayStartHour" type="number" min="0" max="23"></div>
<div><label for="nightStartHour">Ora inizio notte</label><input id="nightStartHour" type="number" min="0" max="23"></div>
<div><label><input class="switch" id="alertsEnabled" type="checkbox"> Soglie e allarmi attivi</label></div>
<div><label><input class="switch" id="historyEnabled" type="checkbox"> Storico dati attivo</label></div>
<div><label><input class="switch" id="useLuxForAuto" type="checkbox"> Usa lux APDS per auto giorno/notte</label></div>
<div><label for="luxNightThreshold">Soglia lux notte</label><input id="luxNightThreshold" type="number" min="0" step="0.5"></div>
<div><label for="luxDayThreshold">Soglia lux giorno</label><input id="luxDayThreshold" type="number" min="1" step="0.5"></div>
</div><div class="form2" style="margin-top:14px">
<div><label>Temp min</label><input id="tempLow" type="number" step="0.1"></div>
<div><label>Temp max</label><input id="tempHigh" type="number" step="0.1"></div>
<div><label>Umid min</label><input id="humLow" type="number" step="0.1"></div>
<div><label>Umid max</label><input id="humHigh" type="number" step="0.1"></div>
<div><label>Press min</label><input id="pressureLow" type="number" step="0.1"></div>
<div><label>Press max</label><input id="pressureHigh" type="number" step="0.1"></div>
<div><label>eCO2 warn</label><input id="eco2Warn" type="number"></div>
<div><label>eCO2 alarm</label><input id="eco2Alarm" type="number"></div>
<div><label>TVOC warn</label><input id="tvocWarn" type="number"></div>
<div><label>TVOC alarm</label><input id="tvocAlarm" type="number"></div>
<div><label>AQI warn</label><input id="aqiWarn" type="number" min="1" max="5"></div>
<div style="display:flex;align-items:end"><button class="saveBtn" onclick="saveConfig()">Salva su ROM</button></div>
</div></div></section>
<section class="page" id="page-ha"><div class="grid">
<div class="card"><div class="label">Export rete</div><div class="small"><a href="/api/data" target="_blank">/api/data</a><br><a href="/api/history" target="_blank">/api/history</a><br><a href="/api/alerts" target="_blank">/api/alerts</a><br><a href="/api/export.csv" target="_blank">/api/export.csv</a></div></div>
<div class="card"><div class="label">Home Assistant YAML</div><textarea id="haYaml"></textarea></div>
</div></section>
</div><script>
const tabs=document.querySelectorAll('.tab');const pages=document.querySelectorAll('.page');tabs.forEach(btn=>btn.addEventListener('click',()=>{tabs.forEach(b=>b.classList.remove('active'));pages.forEach(p=>p.classList.remove('active'));btn.classList.add('active');document.getElementById('page-'+btn.dataset.page).classList.add('active');}));
function fmt(v,d=1){if(v===null||v===undefined||Number.isNaN(v))return '--';return Number(v).toFixed(d)}
function loadAll(){fetch('/api/data').then(r=>r.json()).then(j=>{tempAir.textContent=fmt(j.temperatureAir,1);humAir.textContent=fmt(j.humidityAir,1);pressBmp.textContent=fmt(j.pressureHpa,1);lux.textContent=fmt(j.lux,0);aqiText.textContent=j.aqiText||'--';aqi.textContent=j.aqi??'--';tvoc.textContent=j.tvoc??'--';eco2.textContent=j.eco2??'--';alertSummary.textContent=j.alertSummary||'--';alertCount.textContent=j.alertCount??'0';pageName.textContent=j.pageName||'--';uptime.textContent=j.uptime||'--';wifiChip.textContent='Wi‑Fi: '+(j.wifiConnected?'connesso':'offline');themeChip.textContent='Tema: '+(j.isNightTheme?'notte':'giorno');alertChip.textContent='Allarmi: '+(j.alertCount??0);});fetch('/api/history').then(r=>r.json()).then(j=>{histCount.textContent=j.count??0;});fetch('/api/config').then(r=>r.json()).then(j=>{autoRotateEnabled.checked=!!j.autoRotateEnabled;pageIntervalSec.value=j.pageIntervalSec;sensorIntervalSec.value=j.sensorIntervalSec;dimmerEnabled.checked=!!j.dimmerEnabled;dimmerMode.value=String(j.dimmerMode);dayStartHour.value=j.dayStartHour;nightStartHour.value=j.nightStartHour;alertsEnabled.checked=!!j.alertsEnabled;historyEnabled.checked=!!j.historyEnabled;useLuxForAuto.checked=!!j.useLuxForAuto;luxNightThreshold.value=j.luxNightThreshold;luxDayThreshold.value=j.luxDayThreshold;tempLow.value=j.tempLow;tempHigh.value=j.tempHigh;humLow.value=j.humLow;humHigh.value=j.humHigh;pressureLow.value=j.pressureLow;pressureHigh.value=j.pressureHigh;eco2Warn.value=j.eco2Warn;eco2Alarm.value=j.eco2Alarm;tvocWarn.value=j.tvocWarn;tvocAlarm.value=j.tvocAlarm;aqiWarn.value=j.aqiWarn;});fetch('/api/ha_yaml').then(r=>r.text()).then(t=>{haYaml.value=t;});}
function saveConfig(){const b=new URLSearchParams();['pageIntervalSec','sensorIntervalSec','dimmerMode','dayStartHour','nightStartHour','luxNightThreshold','luxDayThreshold','tempLow','tempHigh','humLow','humHigh','pressureLow','pressureHigh','eco2Warn','eco2Alarm','tvocWarn','tvocAlarm','aqiWarn'].forEach(id=>b.set(id,document.getElementById(id).value));b.set('autoRotateEnabled',autoRotateEnabled.checked?'1':'0');b.set('dimmerEnabled',dimmerEnabled.checked?'1':'0');b.set('alertsEnabled',alertsEnabled.checked?'1':'0');b.set('historyEnabled',historyEnabled.checked?'1':'0');b.set('useLuxForAuto',useLuxForAuto.checked?'1':'0');fetch('/api/config',{method:'POST',body:b}).then(()=>loadAll());}
setInterval(loadAll,3000);loadAll();
</script></body></html>
)rawliteral";

static void handleRoot() { server.send_P(200, "text/html; charset=utf-8", INDEX_HTML); }

void beginWeb() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/data", HTTP_GET, handleApiData);
  server.on("/api/config", HTTP_GET, handleApiConfigGet);
  server.on("/api/config", HTTP_POST, handleApiConfigPost);
  server.on("/api/page", HTTP_POST, handleApiPagePost);
  server.on("/api/alerts", HTTP_GET, handleApiAlerts);
  server.on("/api/history", HTTP_GET, handleApiHistory);
  server.on("/api/export.csv", HTTP_GET, handleApiExportCsv);
  server.on("/api/ha_yaml", HTTP_GET, handleApiHaYaml);
  server.begin();
  Serial.println("[WEB] server avviato");
}

void handleWeb() { server.handleClient(); }
