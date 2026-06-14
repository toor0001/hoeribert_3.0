#include "WebServerManager.h"
#include "AudioPlayer.h"
#include "WebAssets.h"
#include <WiFi.h>

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, viewport-fit=cover">
  <title>ITT SL58 Player</title>
  <style>
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }

    :root {
      --case: #15140f;
      --panel: #24221b;
      --panel-2: #11100c;
      --line: #6b5b32;
      --green: #35ff82;
      --amber: #ffbf3f;
      --red: #ff4f3f;
      --blue: #61c7ff;
      --paper: #f3e2a2;
      --muted: #a99d7c;
      --c64-blue: #40318d;
      --c64-cyan: #67b6bd;
    }
    
    body {
      font-family: Arial, Helvetica, sans-serif;
      background:
        repeating-linear-gradient(0deg, rgba(255,255,255,0.025) 0, rgba(255,255,255,0.025) 1px, transparent 1px, transparent 4px),
        repeating-linear-gradient(90deg, rgba(0,0,0,0.22) 0, rgba(0,0,0,0.22) 2px, transparent 2px, transparent 6px),
        radial-gradient(circle at 20% 10%, rgba(255, 191, 63, 0.18), transparent 32%),
        radial-gradient(circle at 80% 20%, rgba(103, 182, 189, 0.18), transparent 28%),
        linear-gradient(135deg, #20180f 0%, #080907 55%, #17120c 100%);
      min-height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      padding: 20px;
    }
    
    .container {
      background: linear-gradient(180deg, #343026 0%, var(--case) 100%);
      border: 2px solid #5c5139;
      border-radius: 8px;
      box-shadow: 0 24px 70px rgba(0, 0, 0, 0.55), inset 0 1px 0 rgba(255,255,255,0.12);
      padding: 22px;
      max-width: 720px;
      width: 100%;
      color: #fff;
    }
    
    .header {
      display: flex;
      justify-content: space-between;
      align-items: flex-end;
      gap: 16px;
      border-bottom: 2px solid var(--line);
      margin-bottom: 18px;
      padding-bottom: 14px;
    }
    
    .title {
      color: var(--paper);
      font-family: Georgia, 'Times New Roman', serif;
      font-size: 34px;
      font-weight: bold;
      letter-spacing: 0;
      line-height: 1;
      text-shadow: 0 0 10px rgba(255, 191, 63, 0.35);
    }
    
    .subtitle {
      color: var(--muted);
      font-size: 12px;
      margin-top: 6px;
      text-transform: uppercase;
    }

    .badge {
      border: 1px solid var(--amber);
      color: var(--amber);
      font-family: 'Courier New', monospace;
      font-size: 12px;
      padding: 6px 8px;
      text-align: right;
      white-space: nowrap;
    }

    .photo-panel {
      position: relative;
      width: min(100%, 520px);
      margin-bottom: 18px;
      margin-left: auto;
      margin-right: auto;
      aspect-ratio: 520 / 693;
      filter: drop-shadow(0 18px 28px rgba(0,0,0,0.5));
    }

    .device-photo {
      display: block;
      width: 100%;
      height: 100%;
      object-fit: contain;
      user-select: none;
      -webkit-user-drag: none;
    }

    .hotspot {
      position: absolute;
      border: 1px solid transparent;
      border-radius: 5px;
      background: rgba(53, 255, 130, 0);
      cursor: pointer;
      transition: background 0.15s ease, border-color 0.15s ease, box-shadow 0.15s ease;
    }

    .hotspot:hover,
    .hotspot:focus-visible {
      outline: none;
      border-color: rgba(53, 255, 130, 0.9);
      background: rgba(53, 255, 130, 0.16);
      box-shadow: 0 0 14px rgba(53, 255, 130, 0.32), inset 0 0 10px rgba(53, 255, 130, 0.16);
    }

    .hotspot:active {
      background: rgba(255, 191, 63, 0.24);
      border-color: var(--amber);
    }

    .cass-btn { top: 86.0%; left: 31.7%; width: 10.3%; height: 8.3%; }
    .record-btn { top: 86.0%; left: 42.1%; width: 10.3%; height: 8.3%; }
    .rew-btn { top: 86.0%; left: 52.6%; width: 10.3%; height: 8.3%; }
    .play-btn { top: 86.0%; left: 63.1%; width: 10.3%; height: 8.3%; }
    .stop-btn { top: 86.0%; left: 73.5%; width: 10.3%; height: 8.3%; }
    .ff-btn { top: 86.0%; left: 84.0%; width: 10.0%; height: 8.3%; }
    .volume-hotspot { top: 86.3%; left: 6.7%; width: 17.0%; height: 8.8%; }

    .tape-status {
      position: absolute;
      left: 38.5%;
      top: 49.0%;
      width: 27%;
      min-height: 30px;
      display: flex;
      align-items: center;
      justify-content: center;
      color: #1d170d;
      font-family: 'Courier New', monospace;
      font-weight: bold;
      font-size: 11px;
      text-align: center;
      transform: rotate(0.5deg);
      pointer-events: none;
    }

    .panel-meter {
      position: absolute;
      left: 32%;
      top: 79%;
      width: 58%;
      height: 3px;
      background: repeating-linear-gradient(90deg, var(--green) 0 12px, var(--amber) 12px 18px, var(--red) 18px 22px, transparent 22px 24px);
      box-shadow: 0 0 12px rgba(53,255,130,0.5), 0 0 22px rgba(255,191,63,0.28);
      opacity: 0.9;
      pointer-events: none;
    }
    
    .status-box {
      background: var(--panel-2);
      border: 1px solid var(--line);
      padding: 16px;
      margin-bottom: 16px;
    }
    
    .status-item {
      display: flex;
      justify-content: space-between;
      margin-bottom: 12px;
      gap: 18px;
      font-family: 'Courier New', monospace;
      font-size: 14px;
    }

    .status-item:last-child {
      margin-bottom: 0;
    }
    
    .status-label {
      color: var(--muted);
    }
    
    .status-value {
      color: var(--green);
      font-weight: bold;
      text-align: right;
      text-shadow: 0 0 8px rgba(53, 255, 130, 0.45);
    }
    
    .controls {
      display: grid;
      grid-template-columns: 1fr 1fr 1fr;
      gap: 10px;
      margin-bottom: 16px;
    }
    
    .btn {
      background: linear-gradient(180deg, #3a372d 0%, #191713 100%);
      border: 1px solid #7d6c43;
      color: var(--paper);
      padding: 15px;
      border-radius: 4px;
      font-size: 18px;
      cursor: pointer;
      transition: all 0.15s ease;
      font-weight: bold;
      box-shadow: inset 0 1px 0 rgba(255,255,255,0.14), 0 4px 0 #080806;
    }
    
    .btn:hover {
      color: var(--green);
      border-color: var(--green);
      box-shadow: inset 0 1px 0 rgba(255,255,255,0.14), 0 4px 0 #080806, 0 0 14px rgba(53, 255, 130, 0.18);
    }
    
    .btn:active {
      transform: translateY(3px);
      box-shadow: inset 0 1px 0 rgba(255,255,255,0.14), 0 1px 0 #080806;
    }
    
    .btn.wide {
      grid-column: span 3;
    }
    
    .slider-group {
      margin-bottom: 20px;
    }
    
    .slider-label {
      font-family: 'Courier New', monospace;
      font-size: 12px;
      color: var(--muted);
      margin-bottom: 8px;
      display: flex;
      justify-content: space-between;
    }
    
    input[type="range"] {
      width: 100%;
      height: 6px;
      background: #070806;
      border: 1px solid #5d5138;
      outline: none;
      -webkit-appearance: none;
      appearance: none;
    }
    
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 20px;
      height: 20px;
      border-radius: 3px;
      background: var(--amber);
      cursor: pointer;
      box-shadow: 0 0 10px rgba(255, 191, 63, 0.5);
    }
    
    input[type="range"]::-moz-range-thumb {
      width: 20px;
      height: 20px;
      border-radius: 3px;
      background: var(--amber);
      cursor: pointer;
      border: none;
      box-shadow: 0 0 10px rgba(255, 191, 63, 0.5);
    }
    
    .folder-select {
      width: 100%;
      background: #100f0b;
      border: 1px solid var(--line);
      color: var(--paper);
      padding: 10px;
      border-radius: 4px;
      font-size: 14px;
      margin-top: 10px;
    }
    
    .folder-select:focus {
      outline: none;
      border-color: var(--amber);
    }

    .console {
      background: #050604;
      border: 1px solid #31472f;
      color: var(--green);
      font-family: 'Courier New', monospace;
      font-size: 12px;
      min-height: 180px;
      max-height: 220px;
      overflow: hidden;
      padding: 12px;
      box-shadow: inset 0 0 18px rgba(53,255,130,0.08);
      white-space: pre-wrap;
    }

    .console-title {
      color: var(--amber);
      font-family: 'Courier New', monospace;
      font-size: 12px;
      margin: 6px 0 8px;
      text-transform: uppercase;
    }

    @media (max-width: 560px) {
      body {
        min-height: 100svh;
        padding: 0;
        align-items: flex-start;
        background:
          radial-gradient(circle at 50% 12%, rgba(103, 182, 189, 0.24), transparent 38%),
          linear-gradient(180deg, #0b0b0b 0%, #11100c 100%);
      }

      .container {
        min-height: 100svh;
        max-width: none;
        padding: env(safe-area-inset-top) 10px 18px;
        border: 0;
        border-radius: 0;
        box-shadow: none;
        background: linear-gradient(180deg, #090908 0%, #15140f 100%);
      }

      .header {
        position: absolute;
        z-index: 3;
        top: max(10px, env(safe-area-inset-top));
        left: 12px;
        right: 12px;
        padding: 8px 10px;
        margin: 0;
        border: 1px solid rgba(255,191,63,0.35);
        background: rgba(5, 6, 4, 0.72);
        backdrop-filter: blur(4px);
        align-items: center;
      }

      .title {
        font-size: 20px;
      }

      .subtitle {
        display: none;
      }

      .badge {
        font-size: 10px;
        padding: 4px 6px;
      }

      .photo-panel {
        width: min(100vw, calc(100svh * 520 / 693));
        height: min(100svh, calc(100vw * 693 / 520));
        margin: 0 auto 12px;
        filter: drop-shadow(0 10px 18px rgba(0,0,0,0.55));
      }

      .hotspot {
        border-width: 2px;
      }

      .tape-status {
        font-size: 9px;
      }

      .status-box {
        display: grid;
        grid-template-columns: 1fr 1fr;
        gap: 8px 12px;
        padding: 10px;
        margin-bottom: 10px;
      }

      .status-item {
        display: block;
        margin: 0;
        font-size: 11px;
      }

      .status-value {
        display: block;
        margin-top: 2px;
        text-align: left;
      }

      .console {
        min-height: 112px;
        max-height: 132px;
        font-size: 10px;
        padding: 9px;
      }

      .controls {
        display: none;
      }

      .slider-group {
        margin-bottom: 12px;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <div>
        <div class="title">ITT SL58</div>
        <div class="subtitle">Kassettenrekorder-Umbau</div>
      </div>
      <div class="badge">STEREO WEB CONTROL</div>
    </div>

    <div class="photo-panel">
      <img class="device-photo" src="/assets/itt-sl58-panel.webp" alt="ITT SL58 Super">
      <div class="tape-status" id="tape-line">BEREIT</div>
      <div class="panel-meter"></div>
      <button class="hotspot cass-btn" aria-label="Cassette" onclick="control('folder-1')"></button>
      <button class="hotspot record-btn" aria-label="Record" onclick="addClientLog('record taste gedrueckt')"></button>
      <button class="hotspot rew-btn" aria-label="Zurueck" onclick="control('previous')"></button>
      <button class="hotspot play-btn" aria-label="Start" onclick="control('playpause')"></button>
      <button class="hotspot stop-btn" aria-label="Stop" onclick="control('stop')"></button>
      <button class="hotspot ff-btn" aria-label="Vor" onclick="control('next')"></button>
      <button class="hotspot volume-hotspot" aria-label="Volume" onclick="addClientLog('volume regler am geraet')"></button>
    </div>
    
    <div class="status-box">
      <div class="status-item">
        <span class="status-label">Status:</span>
        <span class="status-value" id="status">Verbindung...</span>
      </div>
      <div class="status-item">
        <span class="status-label">Ordner:</span>
        <span class="status-value" id="folder">-</span>
      </div>
      <div class="status-item">
        <span class="status-label">Track:</span>
        <span class="status-value" id="track">-</span>
      </div>
      <div class="status-item">
        <span class="status-label">Dateianzahl:</span>
        <span class="status-value" id="filecount">-</span>
      </div>
      <div class="status-item">
        <span class="status-label">Zeit:</span>
        <span class="status-value" id="seconds">-</span>
      </div>
    </div>

    <div class="console-title">Konsole</div>
    <div class="console" id="console">warte auf daten...</div>
    
    <div class="slider-group">
      <div class="slider-label">
        <span>Lautstärke</span>
        <span id="volvalue">15</span>
      </div>
      <input type="range" id="volume" min="0" max="30" value="15">
    </div>
    
    <div class="controls">
      <button class="btn" onclick="control('previous')">⏮</button>
      <button class="btn" onclick="control('playpause')">▶</button>
      <button class="btn" onclick="control('next')">⏭</button>
      <button class="btn wide" onclick="control('stop')">⏹ STOP</button>
    </div>
    
    <div class="slider-group">
      <div class="slider-label">Ordner wählen</div>
      <select class="folder-select" id="folder-select" onchange="changeFolder()">
        <option value="">-- Ordner auswählen --</option>
      </select>
    </div>
  </div>

  <script>
    const folderNames = [
      'Der Super-Papagei',
      'Der Phantomsee',
      'Der Karpatenhund',
      'Die schwarze Katze',
      'Der Fluch des Rubins',
      'Der sprechende Totenkopf',
      'Der unheimliche Drache',
      'Der grüne Geist',
      'Die rätselhaften Bilder',
      'Und die flüsternde Mumie',
      'Und das Gespensterschloss',
      'Und der seltsame Wecker',
      'Und der lachende Schatten',
      'Und der schreiende Nebel',
      'Und der rasende Löwe',
      'Und der Zauberspiegel',
      'Und die gefährliche Erbschaft',
      'Und die Geisterinsel',
      'Und der Teufelsberg',
      'Und die flammende Spur',
      'Und der tanzende Teufel',
      'Und der verschwundene Schatz',
      'Und das Aztekenschwert',
      'Und die silberne Spinne',
      'Und die singende Schlange',
      'Und die Silbermine',
      'Und der magische Kreis',
      'Und der Doppelgänger',
      'Und die falschen Detektive',
      'Und das Riff der Haie',
      'Und das Narbengesicht',
      'Und der Ameisenmensch',
      'Und die bedrohte Ranch',
      'Und der rote Pirat',
      'Und der Höhlenmensch',
      'Und der Super-Wal',
      'Und der heimliche Hehler',
      'Und der unsichtbare Gegner',
      'Und die Perlenvögel',
      'Und der Automarder',
      'Und das Volk der Winde',
      'Und der weinende Sarg',
      'Und der höllische Werwolf',
      'Und der gestohlene Preis',
      'Und das Gold der Wikinger',
      'Und der schrullige Millionär',
      'Und der giftige Gockel',
      'Und die gefährlichen Fässer',
      'Und die Comic-Diebe',
      'Und der verschwundene Filmstar',
      'Und der riskante Ritt',
      'Und die Musikpiraten',
      'Und die Automafia',
      'Und der rote Rächer',
      'Und der verrückte Maler',
      'Und der verschwundene Fußballer',
      'Tatort Zirkus',
      'Und der verrückte Erfinder',
      'Und die Rache des Tigers',
      'Und die Geisterbahn',
      'Und die Rache des Untoten',
      'Spuk im Hotel',
      'Fußball-Gangster',
      'Geisterstadt',
      'Diamantenschmuggel',
      'Die Schattenmänner',
      'Das Geheimnis der Särge',
      'Schatz im Bergsee',
      'Späte Rache',
      'Schüsse aus dem Dunkel',
      'Die verschwundene Seglerin',
      'Dreckiger Deal',
      'Poltergeist',
      'Das brennende Schwert',
      'Die Spur des Raben',
      'Stimmen aus dem Nichts',
      'Pistenteufel',
      'Das leere Grab',
      'Im Bann des Voodoo',
      'Geheimnis der Karten',
      'Verdeckte Fouls',
      'Die Karten des Bösen',
      'Meuterei auf hoher See',
      'Musik des Teufels',
      'Feuerturm',
      'Nacht in Angst',
      'Wolfsgesicht',
      'Vampir im Internet',
      'Tödliche Spur',
      'Der Feuerteufel',
      'Labyrinth der Götter',
      'Todesflug',
      'Das schwarze Monster',
      'Botschaft von Geisterhand',
      'Rufmord',
      'Das rote Phantom',
      'Insektenstachel',
      'Tal des Schreckens',
      'Ruf der Krähen'
    ];

    function populateFolders() {
      const select = document.getElementById('folder-select');
      folderNames.forEach((name, index) => {
        const option = document.createElement('option');
        const folder = index + 1;
        option.value = folder;
        option.textContent = String(folder).padStart(2, '0') + ' - ' + name;
        select.appendChild(option);
      });
    }

    function addClientLog(text) {
      const now = new Date();
      const stamp = now.toLocaleTimeString('de-DE', { hour12: false });
      const box = document.getElementById('console');
      const lines = box.textContent === 'warte auf daten...' ? [] : box.textContent.split('\n');
      lines.push('[' + stamp + '] WEB ' + text);
      while (lines.length > 18) lines.shift();
      box.textContent = lines.join('\n');
    }

    function formatSeconds(value) {
      const seconds = Number(value || 0);
      const minutes = Math.floor(seconds / 60);
      const rest = seconds % 60;
      return minutes + ':' + String(rest).padStart(2, '0');
    }

    function updateStatus() {
      fetch('/api/status')
        .then(r => r.json())
        .then(data => {
          document.getElementById('status').textContent = data.status;
          document.getElementById('folder').textContent = data.folder || '-';
          document.getElementById('track').textContent = data.track || '-';
          document.getElementById('filecount').textContent = data.fileCount || '-';
          document.getElementById('seconds').textContent = formatSeconds(data.seconds);
          document.getElementById('tape-line').textContent = data.folder ? ('ORDNER ' + data.folder + ' / TRACK ' + (data.track || '-')) : 'BEREIT';
          document.getElementById('volume').value = data.volume || 15;
          document.getElementById('volvalue').textContent = data.volume || 15;
        })
        .catch(e => addClientLog('status fehler: ' + e.message));
    }

    function updateLogs() {
      fetch('/api/logs')
        .then(r => r.json())
        .then(data => {
          if (data.lines && data.lines.length) {
            document.getElementById('console').textContent = data.lines.join('\n');
          }
        })
        .catch(e => addClientLog('log fehler: ' + e.message));
    }
    
    function control(cmd) {
      fetch('/api/control?cmd=' + cmd)
        .then(r => r.json())
        .then(data => {
          addClientLog('befehl gesendet: ' + cmd);
          setTimeout(updateStatus, 500);
          setTimeout(updateLogs, 600);
        });
    }
    
    function changeFolder() {
      const folder = document.getElementById('folder-select').value;
      if (folder) {
        control('folder-' + folder);
      }
    }
    
    document.getElementById('volume').addEventListener('input', function() {
      document.getElementById('volvalue').textContent = this.value;
      fetch('/api/volume?vol=' + this.value)
        .then(() => updateLogs())
        .catch(e => addClientLog('volume fehler: ' + e.message));
    });
    
    populateFolders();
    updateStatus();
    updateLogs();
    setInterval(updateStatus, 1000);
    setInterval(updateLogs, 1500);
  </script>
</body>
</html>
)rawliteral";

void WebServerManager::begin(const char* ssid, const char* password, AudioPlayer* audioPlayer) {
  player = audioPlayer;
  
  WiFi.begin(ssid, password);
  appendLog("[WEB] WiFi verbinden...");
  Serial.println("[WEB] WiFi verbinden...");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    appendLog("[WEB] WiFi verbunden");
    appendLog("[WEB] IP: " + WiFi.localIP().toString());
    appendLog("[WEB] Web-Interface: http://" + WiFi.localIP().toString());
    Serial.println("\n[WEB] WiFi verbunden!");
    Serial.println("[WEB] IP: " + WiFi.localIP().toString());
    Serial.println("[WEB] Web-Interface: http://" + WiFi.localIP().toString());
    
    server.on("/", [this]() { handleRoot(); });
    server.on("/assets/itt-sl58-panel.webp", [this]() { handlePanelImage(); });
    server.on("/api/status", [this]() { handleStatus(); });
    server.on("/api/logs", [this]() { handleLogs(); });
    server.on("/api/control", [this]() { handleControl(); });
    server.on("/api/volume", HTTP_GET, [this]() {
      if (!player) {
        appendLog("[WEB] Volume Fehler: Player nicht bereit");
        server.send(503, "application/json", "{\"error\":\"player unavailable\"}");
        return;
      }

      if (!server.hasArg("vol")) {
        appendLog("[WEB] Volume Fehler: Parameter fehlt");
        server.send(400, "application/json", "{\"error\":\"missing vol\"}");
        return;
      }

      int vol = server.arg("vol").toInt();
      player->setVolume(constrain(vol, 0, 30));
      appendLog("[WEB] Volume " + String(constrain(vol, 0, 30)));
      server.send(200, "application/json", "{\"ok\":true}");
    });
    
    server.begin();
    initialized = true;
    appendLog("[WEB] Server gestartet");
    Serial.println("[WEB] Server gestartet!");
  } else {
    appendLog("[WEB] WiFi-Verbindung fehlgeschlagen");
    Serial.println("[WEB] ✗ WiFi-Verbindung fehlgeschlagen");
  }
}

void WebServerManager::update() {
  if (initialized) {
    server.handleClient();
  }
}

void WebServerManager::handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void WebServerManager::handlePanelImage() {
  server.send_P(200, "image/webp", reinterpret_cast<const char*>(ITT_SL58_PANEL_WEBP), ITT_SL58_PANEL_WEBP_len);
}

void WebServerManager::handleStatus() {
  server.send(200, "application/json", getStatusJSON());
}

void WebServerManager::handleLogs() {
  server.send(200, "application/json", getLogsJSON());
}

String WebServerManager::getStatusJSON() const {
  if (!player) {
    return "{\"status\":\"Fehler\",\"folder\":0,\"track\":0,\"volume\":0,\"fileCount\":0}";
  }
  
  String json = "{";
  json += "\"status\":\"" + getStatusText(player->isPlayingNow() ? 1 : 0) + "\",";
  
  PlaybackPosition pos = player->getPlaybackPosition();
  json += "\"folder\":" + String(pos.folder) + ",";
  json += "\"track\":" + String(pos.track) + ",";
  json += "\"seconds\":" + String(pos.seconds) + ",";
  
  AudioPlayerStatus status = player->readStatus();
  json += "\"volume\":" + String(status.volume) + ",";
  json += "\"fileCount\":" + String(status.fileCount);
  json += "}";
  
  return json;
}

String WebServerManager::getStatusText(int state) const {
  return state ? "▶ Wiedergabe" : "⏸ Pause";
}

void WebServerManager::handleControl() {
  if (!server.hasArg("cmd") || !player) {
    appendLog("[WEB] Control Fehler: cmd fehlt");
    server.send(400, "application/json", "{\"error\":\"missing cmd\"}");
    return;
  }
  
  String cmd = server.arg("cmd");
  
  if (cmd == "playpause") {
    if (player->isPlayingNow()) {
      player->pause();
      appendLog("[WEB] Pause");
    } else {
      player->resume();
      appendLog("[WEB] Play");
    }
  } else if (cmd == "play") {
    player->resume();
    appendLog("[WEB] Play");
  } else if (cmd == "pause") {
    player->pause();
    appendLog("[WEB] Pause");
  } else if (cmd == "stop") {
    player->stop();
    appendLog("[WEB] Stop");
  } else if (cmd == "next") {
    player->next();
    appendLog("[WEB] Next");
  } else if (cmd == "previous") {
    player->previous();
    appendLog("[WEB] Previous");
  } else if (cmd.startsWith("folder-")) {
    String folderStr = cmd.substring(7);
    int folder = folderStr.toInt();
    if (folder > 0 && folder <= 99) {
      player->playFolder(folder);
      appendLog("[WEB] Ordner " + String(folder));
    }
  } else {
    appendLog("[WEB] Unbekannter Befehl: " + cmd);
  }
  
  server.send(200, "application/json", "{\"ok\":true}");
}

String WebServerManager::getLogsJSON() const {
  String json = "{\"lines\":[";

  for (int i = 0; i < storedLogLines; i++) {
    int index = nextLogLine - storedLogLines + i;
    while (index < 0) {
      index += LOG_LINE_COUNT;
    }
    index %= LOG_LINE_COUNT;

    if (i > 0) {
      json += ",";
    }
    json += "\"";
    json += jsonEscape(logLines[index]);
    json += "\"";
  }

  json += "]}";
  return json;
}

String WebServerManager::jsonEscape(const String& value) const {
  String escaped = "";
  escaped.reserve(value.length() + 8);

  for (int i = 0; i < value.length(); i++) {
    char c = value.charAt(i);
    if (c == '"' || c == '\\') {
      escaped += '\\';
    }
    escaped += c;
  }

  return escaped;
}

void WebServerManager::appendLog(const String& line) {
  unsigned long seconds = millis() / 1000UL;
  String stamped = "[" + String(seconds) + "s] " + line;
  logLines[nextLogLine] = stamped;
  nextLogLine = (nextLogLine + 1) % LOG_LINE_COUNT;
  if (storedLogLines < LOG_LINE_COUNT) {
    storedLogLines++;
  }
}
