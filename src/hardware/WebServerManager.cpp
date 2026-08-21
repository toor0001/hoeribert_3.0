#include "WebServerManager.h"

#include "AudioPlayer.h"
#include "RFIDManager.h"

#include <ArduinoOTA.h>

namespace {
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 10000;

const char MAINTENANCE_PAGE[] PROGMEM = R"html(
<!doctype html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Höribert 3.0 – Wartungsmodus</title>
<style>body{font:14px system-ui;margin:20px;max-width:1000px;background:#111;color:#eee}
h1,h2{color:#8fda9d}pre{background:#000;padding:12px;overflow:auto;white-space:pre-wrap}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:12px}
section{border:1px solid #555;padding:12px}dt{color:#aaa}dd{margin:0 0 6px}
.controls{display:flex;flex-wrap:wrap;gap:8px;align-items:center}select,button{font:inherit;padding:8px}
#player-result{min-height:1.4em;color:#aaa}</style></head>
<body><h1>Höribert 3.0 – Wartungsmodus</h1><div id="status">Lade Status…</div>
<section><h2>PLAYER-STEUERUNG</h2><div class="controls">
<label for="player-folder">Hörbuch / Ordner</label><select id="player-folder"></select>
<button type="button" data-action="start">START</button>
<button type="button" data-action="pause">STOP / PAUSE</button>
<button type="button" data-action="previous">ZURÜCK</button>
<button type="button" data-action="next">VOR</button></div><p id="player-result"></p></section>
<h2>Live Log</h2><pre id="log">Lade Log…</pre><script>
const esc=s=>String(s??'').replace(/[&<>]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]));
const folderNames={};
const folderLabel=n=>folderNames[n]||('Ordner '+String(n).padStart(2,'0'));
const folderSelect=document.querySelector('#player-folder');
for(let n=1;n<=99;n++){let option=document.createElement('option');option.value=n;option.textContent=folderLabel(n);folderSelect.append(option)}
async function playerAction(action){let url='/api/player/'+action;if(action==='start')url+='?folder='+encodeURIComponent(folderSelect.value);
let result=document.querySelector('#player-result');try{let response=await fetch(url,{method:'POST'});let body=await response.json();
result.textContent=body.message||('HTTP '+response.status);await refresh()}catch(e){result.textContent='Fehler: '+e}}
document.querySelectorAll('[data-action]').forEach(button=>button.addEventListener('click',()=>playerAction(button.dataset.action)));
async function refresh(){try{let s=await (await fetch('/api/status')).json();
let groups={SYSTEM:['uptime','build','maintenance','wifi','ip','rssi'],
RFID:['rc522','rc522Version','uid','cardType','rfidError','tonuino'],
DFPLAYER:['dfReady','dfState','folder','track','trackCount','volume'],
PLAYER:['activeUid','activeFolder','waitingForPlay','sleepTimer'],
'GPIO / BEDIENUNG':['playButton','forwardButton','backButton','timerButton','volumeRaw','logicalVolume']};
let out='<div class="grid">';for(const [g,keys] of Object.entries(groups)){out+='<section><h2>'+g+'</h2><dl>';
for(const k of keys)out+='<dt>'+esc(k)+'</dt><dd>'+esc(s[k])+'</dd>';out+='</dl></section>'}out+='</div>';
document.querySelector('#status').innerHTML=out;let l=await (await fetch('/api/logs')).json();
document.querySelector('#log').textContent=(l.lines||[]).join('\n');}catch(e){document.querySelector('#status').textContent=e}}
refresh();setInterval(refresh,1500);</script></body></html>)html";
}

void WebServerManager::begin(const char* ssid, const char* password, const char* otaName,
                             AudioPlayer* audioPlayer, RFIDManager* rfidManager) {
  player = audioPlayer;
  rfid = rfidManager;
  initialized = true;
  servicesStarted = false;
  connectTimeoutLogged = false;
  wifiStartedAt = millis();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  ArduinoOTA.setHostname(otaName);
  log("[WARTUNG] WLAN-Verbindung gestartet (nicht blockierend)");
}

void WebServerManager::disable() {
  initialized = false;
  servicesStarted = false;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("[WARTUNG] WLAN, HTTP, OTA und TCP-Log deaktiviert");
}

void WebServerManager::shutdown() {
  if (servicesStarted) {
    server.stop();
    logStreamServer.stop();
    for (auto& client : logStreamClients) client.stop();
    ArduinoOTA.end();
  }
  disable();
}

void WebServerManager::setSnapshot(const MaintenanceSnapshot& value) { snapshot = value; }

void WebServerManager::update() {
  if (!initialized) return;
  if (!servicesStarted && WiFi.status() == WL_CONNECTED) startNetworkServices();
  if (!servicesStarted && !connectTimeoutLogged &&
      millis() - wifiStartedAt >= WIFI_CONNECT_TIMEOUT_MS) {
    connectTimeoutLogged = true;
    log("[WARTUNG] WLAN nach 10s nicht verbunden; Player laeuft weiter");
  }
  if (servicesStarted) {
    server.handleClient();
    updateLogStream();
    ArduinoOTA.handle();
  }
}

void WebServerManager::startNetworkServices() {
  server.on("/", [this]() { handleRoot(); });
  server.on("/api/status", [this]() { handleStatus(); });
  server.on("/api/logs", [this]() { handleLogs(); });
  server.on("/api/player/start", HTTP_POST, [this]() { handlePlayerStart(); });
  server.on("/api/player/pause", HTTP_POST, [this]() { handlePlayerPause(); });
  server.on("/api/player/previous", HTTP_POST, [this]() { handlePlayerPrevious(); });
  server.on("/api/player/next", HTTP_POST, [this]() { handlePlayerNext(); });
  server.begin();
  logStreamServer.begin();
  ArduinoOTA.onStart([this]() { log("[OTA] Update gestartet"); });
  ArduinoOTA.onEnd([this]() { log("[OTA] Update beendet"); });
  ArduinoOTA.onError([this](ota_error_t error) {
    log("[OTA] Fehler " + String(static_cast<int>(error)));
  });
  ArduinoOTA.begin();
  servicesStarted = true;
  log("[WARTUNG] HTTP: http://" + WiFi.localIP().toString());
  log("[WARTUNG] OTA und TCP-Log Port " + String(LOG_STREAM_PORT) + " bereit");
}

void WebServerManager::handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", MAINTENANCE_PAGE);
}

void WebServerManager::handleStatus() { server.send(200, "application/json", getStatusJSON()); }
void WebServerManager::handleLogs() { server.send(200, "application/json", getLogsJSON()); }

void WebServerManager::handlePlayerStart() {
  if (!player || !player->isReady()) {
    sendPlayerActionResult(503, "DFPlayer nicht bereit");
    return;
  }
  if (!server.hasArg("folder")) {
    sendPlayerActionResult(400, "Ordner fehlt");
    return;
  }

  int folder = server.arg("folder").toInt();
  if (folder < 1 || folder > 99) {
    sendPlayerActionResult(400, "Ordner muss zwischen 01 und 99 liegen");
    return;
  }

  String message = "Starte Ordner " + String(folder) + " Track 1";
  logPlayerAction(message);
  player->playFolderTrack(static_cast<uint8_t>(folder), 1, "WEB");
  sendPlayerActionResult(200, message);
}

void WebServerManager::handlePlayerPause() {
  if (!player || !player->isReady()) {
    sendPlayerActionResult(503, "DFPlayer nicht bereit");
    return;
  }
  logPlayerAction("Pause");
  player->pause();
  sendPlayerActionResult(200, "Pause");
}

void WebServerManager::handlePlayerPrevious() {
  if (!player || !player->isReady()) {
    sendPlayerActionResult(503, "DFPlayer nicht bereit");
    return;
  }
  logPlayerAction("Zurück");
  player->previous();
  sendPlayerActionResult(200, "Zurück");
}

void WebServerManager::handlePlayerNext() {
  if (!player || !player->isReady()) {
    sendPlayerActionResult(503, "DFPlayer nicht bereit");
    return;
  }
  logPlayerAction("Vor");
  player->next();
  sendPlayerActionResult(200, "Vor");
}

void WebServerManager::sendPlayerActionResult(int statusCode, const String& message) {
  server.send(statusCode, "application/json",
              "{\"message\":\"" + jsonEscape(message) + "\"}");
}

void WebServerManager::logPlayerAction(const String& message) {
  String line = "[WEB] " + message;
  Serial.println(line);
  log(line);
}

String WebServerManager::getStatusJSON() const {
  AudioPlayerStatus audio = player ? player->getCachedStatus() : AudioPlayerStatus{};
  String dfState = audio.state == 1 ? "Playing" : (audio.folder > 0 ? "Paused" : "Stopped");
  int32_t sleepDeltaMs = snapshot.sleepTimerEndsAt == 0
                             ? 0
                             : static_cast<int32_t>(snapshot.sleepTimerEndsAt - millis());
  long sleepSeconds = max(0L, static_cast<long>(sleepDeltaMs / 1000));
  String json = "{";
  json += "\"uptime\":\"" + String(millis() / 1000UL) + " s\",";
  json += "\"build\":\"" __DATE__ " " __TIME__ "\",";
  json += "\"maintenance\":\"aktiv\",";
  json += "\"wifi\":\"" + String(WiFi.status() == WL_CONNECTED ? "ja" : "nein") + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"rssi\":\"" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0) + " dBm\",";
  json += "\"rc522\":\"" + String(rfid && rfid->isReaderConnected() ? "ja" : "nein") + "\",";
  json += "\"rc522Version\":\"" + String(rfid ? rfid->getReaderVersionText() : "-") + "\",";
  json += "\"uid\":\"" + jsonEscape(rfid ? rfid->getLastUid() : "") + "\",";
  json += "\"cardType\":\"" + jsonEscape(rfid ? rfid->getLastCardType() : "") + "\",";
  json += "\"rfidError\":\"" + jsonEscape(rfid ? rfid->getLastError() : "") + "\",";
  json += "\"tonuino\":\"Folder " + String(snapshot.lastTonuinoFolder) +
          " / Mode " + String(snapshot.lastTonuinoMode) + "\",";
  json += "\"dfReady\":\"" + String(player && player->isReady() ? "ja" : "nein") + "\",";
  json += "\"dfState\":\"" + dfState + "\",";
  json += "\"folder\":" + String(audio.folder) + ",\"track\":" + String(audio.track) + ",";
  json += "\"trackCount\":" + String(audio.tracksInFolder) + ",\"volume\":" + String(audio.volume) + ",";
  json += "\"activeUid\":\"" + jsonEscape(snapshot.activeCardUid) + "\",";
  json += "\"activeFolder\":" + String(snapshot.currentFolder) + ",";
  json += "\"waitingForPlay\":\"" + String(snapshot.waitingForPlay ? "ja" : "nein") + "\",";
  json += "\"sleepTimer\":\"" + String(sleepSeconds) + " s\",";
  json += "\"playButton\":\"" + String(snapshot.playButton ? "gedrueckt" : "frei") + "\",";
  json += "\"forwardButton\":\"" + String(snapshot.forwardButton ? "gedrueckt" : "frei") + "\",";
  json += "\"backButton\":\"" + String(snapshot.backButton ? "gedrueckt" : "frei") + "\",";
  json += "\"timerButton\":\"" + String(snapshot.timerButton ? "gedrueckt" : "frei") + "\",";
  json += "\"volumeRaw\":" + String(snapshot.volumeRaw) + ",";
  json += "\"logicalVolume\":" + String(snapshot.logicalVolume) + "}";
  return json;
}

String WebServerManager::getLogsJSON() const {
  String json = "{\"lines\":[";
  for (int i = 0; i < storedLogLines; i++) {
    int index = nextLogLine - storedLogLines + i;
    while (index < 0) index += LOG_LINE_COUNT;
    index %= LOG_LINE_COUNT;
    if (i > 0) json += ",";
    json += "\"" + jsonEscape(logLines[index]) + "\"";
  }
  return json + "]}";
}

String WebServerManager::jsonEscape(const String& value) const {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (int i = 0; i < value.length(); i++) {
    char c = value.charAt(i);
    if (c == '"' || c == '\\') escaped += '\\';
    if (c == '\n') escaped += "\\n";
    else if (c != '\r') escaped += c;
  }
  return escaped;
}

void WebServerManager::updateLogStream() {
  WiFiClient newClient = logStreamServer.available();
  if (newClient) {
    for (auto& client : logStreamClients) {
      if (!client || !client.connected()) {
        client = newClient;
        client.println("Hoeribert 3.0 Live-Log");
        for (int j = 0; j < storedLogLines; j++) {
          int index = nextLogLine - storedLogLines + j;
          while (index < 0) index += LOG_LINE_COUNT;
          client.println(logLines[index % LOG_LINE_COUNT]);
        }
        return;
      }
    }
    newClient.println("Log-Konsole belegt");
    newClient.stop();
  }
  for (auto& client : logStreamClients) {
    if (client && !client.connected()) client.stop();
  }
}

void WebServerManager::sendLogStreamLine(const String& line) {
  for (auto& client : logStreamClients) {
    if (client && client.connected()) client.println(line);
  }
}

void WebServerManager::log(const String& line) {
  if (!initialized) return;
  String stamped = "[" + String(millis() / 1000UL) + "s] " + line;
  logLines[nextLogLine] = stamped;
  nextLogLine = (nextLogLine + 1) % LOG_LINE_COUNT;
  if (storedLogLines < LOG_LINE_COUNT) storedLogLines++;
  sendLogStreamLine(stamped);
}
