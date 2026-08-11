# 🎧 Höribert 3.0

> Ein moderner RFID-Hörspielplayer im Gehäuse eines klassischen **ITT SL59 Kassettenrecorders**.

Höribert 3.0 verbindet die Haptik eines alten Kassettenrecorders mit einem ESP32, RFID-Karten und einem DFPlayer Mini. Die ursprünglichen Bedienelemente des Recorders werden soweit möglich weiterverwendet – im Inneren arbeitet heute jedoch ein vollständig digitaler Audioplayer.

Der aktuelle Aufbau verwendet einen **Lolin32 Lite**, einen **DFPlayer Mini**, einen **RC522 RFID-Reader** und eine **18650-Zelle**. Die Firmware basiert auf Arduino/PlatformIO und unterstützt zusätzlich WLAN, OTA-Updates, eine Live-Log-Konsole und mehrere Hardware-Test-Firmwares.

---

## ✨ Funktionen

- 🎴 **RFID-gesteuerte Hörspielauswahl** über TonUINO-kompatible Karten
- 🔊 **DFPlayer Mini** als Audioplayer
- 🎚️ Nutzung des originalen Lautstärkereglers über den ESP32-ADC
- ▶️ Play/Pause über die Recorder-Bedienung
- ⏮️ / ⏭️ vorheriger und nächster Track
- ⏲️ Sleep-Timer in 10-Minuten-Schritten
- 💡 Status-LED zur Rückmeldung
- 💾 lokale Bookmark-Infrastruktur im ESP32-NVS
- 📡 WLAN für Diagnose und Wartung
- 🔄 OTA-Firmwareupdates
- 🖥️ serielle Diagnose mit 115200 Baud
- 📟 Live-Logstream über TCP Port `2323`
- 🧪 separate Test-Firmwares für RFID, DFPlayer und GPIO-Funktionen

---

## 🧠 Wie Höribert funktioniert

Eine RFID-Karte enthält TonUINO-kompatible Metadaten. Höribert liest die Karte mit dem RC522 ein und verwendet die hinterlegte Ordnernummer zur Auswahl des entsprechenden Verzeichnisses auf der microSD-Karte des DFPlayers.

Aktuell wird bewusst nur **TonUINO Mode 2 (Album/Ordner)** abgespielt. Die Tracks eines Ordners werden dabei der Reihe nach wiedergegeben.

```text
RFID-Karte
    │
    ▼
RC522
    │
    ▼
Lolin32 Lite / ESP32
    │
    ├── Bedienelemente des ITT SL59
    ├── WLAN / OTA / Diagnose
    │
    ▼
DFPlayer Mini
    │
    ▼
Lautsprecher
```

---

## 🧰 Aktuelle Hardware

| Komponente | Verwendung |
|---|---|
| **ITT SL59** | Originalgehäuse und Bedienelemente |
| **Lolin32 Lite** | ESP32-Hauptcontroller |
| **DFPlayer Mini** | MP3-Wiedergabe von microSD |
| **RC522** | RFID/NFC-Leser |
| **18650 Li-Ion-Zelle** | mobile Stromversorgung |
| Lautstärkepotentiometer | originale Lautstärkeregelung |
| Hall-/Schaltsignale | Vor / Zurück / Play-Pause |
| Status-LED | optische Rückmeldung |

> [!NOTE]
> Ältere Dateien und Kommentare im Repository enthalten teilweise noch die Bezeichnung **ITT SL58**. Der aktuell verwendete Recorder ist ein **ITT SL59**.

---

## 🔌 Pinbelegung der aktuellen Firmware

Die folgende Tabelle basiert auf dem **aktuellen Code** und ist damit maßgeblich für den derzeitigen Firmwarestand.

### RC522 RFID

| RC522 | Lolin32 Lite |
|---|---:|
| SDA / SS | GPIO 5 |
| RST | GPIO 22 |
| SCK | GPIO 18 |
| MISO | GPIO 19 |
| MOSI | GPIO 23 |
| 3.3 V | 3V3 |
| GND | GND |

### DFPlayer Mini

| DFPlayer | Lolin32 Lite |
|---|---:|
| TX | GPIO 16 |
| RX | GPIO 17 |
| VCC | Versorgung |
| GND | GND |

Für die Verbindung **ESP32 TX → DFPlayer RX** wird im Aufbau ein Serienwiderstand verwendet.

### Bedienelemente

| Funktion | GPIO | Verhalten |
|---|---:|---|
| Lautstärke | GPIO 34 | ADC, 0–10 logische Lautstärkestufen |
| Play/Pause | GPIO 26 | aktiv gegen GND |
| Vor | GPIO 14 | nächster Track |
| Zurück | GPIO 13 | vorheriger Track |
| Sleep-Timer | GPIO 25 | kurzer / langer Tastendruck |
| Status-LED | GPIO 32 | kurze optische Rückmeldung |

> [!WARNING]
> `test/DOKU` enthält teilweise noch ältere RC522-Pinbelegungen. Für den aktuellen Stand gelten die Pins aus `src/hardware/RFIDManager.h` bzw. die Tabelle oben.

---

## 🎴 RFID / TonUINO-Kompatibilität

Der RFID-Code erkennt aktuell:

- MIFARE Classic 1K
- MIFARE Classic 4K
- MIFARE Ultralight / kompatible Tags

TonUINO-Karten werden anhand des Magic Cookies

```text
13 37 B3 47
```

erkannt. Ausgelesen werden unter anderem:

```text
Version
Folder
Mode
Special
Special2
```

Für MIFARE Classic wird der TonUINO-Datenblock mit dem Standard-Key `FF FF FF FF FF FF` gelesen.

### Aktuelle Einschränkung

Die NormalMode-Logik unterstützt momentan nur:

```text
Mode 2 = Album / Ordner
```

Andere TonUINO-Modi werden erkannt, aber derzeit nicht abgespielt.

---

## 🎵 Struktur der microSD-Karte

Der DFPlayer verwendet seine normale Ordner-/Track-Struktur. Eine RFID-Karte verweist auf die entsprechende Ordnernummer.

Beispiel:

```text
/01/001.mp3
/01/002.mp3
/01/003.mp3

/02/001.mp3
/02/002.mp3
...
```

Die Firmware enthält aktuell Namen für die Hörspielordner `1` bis `99` und zeigt diese in Diagnoseausgaben entsprechend an.

---

## 🎚️ Lautstärke

Das originale Potentiometer wird über **GPIO 34** eingelesen. Die Firmware filtert und stabilisiert den ADC-Wert, um Sprünge und Störungen des alten Potentiometers zu reduzieren.

Nach außen arbeitet Höribert mit Lautstärkestufen von:

```text
0 … 10
```

Diese werden intern auf den DFPlayer-Bereich bis maximal `24` umgesetzt.

---

## ⏲️ Sleep-Timer

Der Taster an **GPIO 25** steuert den Sleep-Timer:

- **kurzer Tastendruck:** +10 Minuten
- **weiterer kurzer Druck:** nochmals +10 Minuten
- **langer Tastendruck ab ca. 1,2 s:** Timer löschen

Nach Ablauf wird die aktuelle Position gespeichert und die Wiedergabe gestoppt.

---

## 💾 Bookmark-Status

Im Code existiert bereits eine lokale Bookmark-Infrastruktur auf Basis der ESP32-`Preferences`.

Positionen werden unter anderem beim

- Pausieren,
- Kartenwechsel und
- Ablaufen des Sleep-Timers

gespeichert.

**Aktueller Entwicklungsstand:** Das automatische Fortsetzen einer später erneut aufgelegten Karte ist noch nicht vollständig in den Wiedergabestart eingebunden. Der normale Kartenstart beginnt derzeit wieder bei **Track 1**.

---

## 📡 WLAN, OTA und Live-Diagnose

Höribert verbindet sich beim Start mit dem konfigurierten WLAN. Darüber stehen aktuell vor allem Wartungs- und Diagnosefunktionen zur Verfügung.

### OTA

ArduinoOTA wird automatisch gestartet, sobald die WLAN-Verbindung hergestellt wurde.

Die PlatformIO-Konfiguration enthält sowohl USB- als auch OTA-Umgebungen.

### Live-Log

Zusätzlich zum USB-Serial-Monitor kann die laufende Firmware über TCP beobachtet werden:

```bash
nc <IP-DES-HOERIBERT> 2323
```

Bis zu zwei Log-Clients können gleichzeitig verbunden sein.

### Webinterface

Im Repository ist bereits ein umfangreiches, optisch an den Recorder angelehntes Webinterface implementiert.

**Im aktuellen Firmwarestand ist der HTTP-Server jedoch absichtlich deaktiviert:**

```cpp
constexpr bool HTTP_SERVER_ENABLED = false;
```

WLAN, OTA und der Live-Logstream funktionieren davon unabhängig weiter.

---

## 🏗️ Software-Architektur

Der produktive Einstiegspunkt ist bewusst sehr klein:

```text
src/main.cpp
    │
    ▼
App
    │
    ▼
NormalMode
    │
    ├── RFIDManager
    ├── AudioPlayer
    └── WebServerManager
```

Wichtige Dateien:

```text
src/
├── main.cpp
├── App.cpp
├── App.h
│
├── modes/
│   ├── NormalMode.cpp
│   └── NormalMode.h
│
└── hardware/
    ├── AudioPlayer.cpp
    ├── AudioPlayer.h
    ├── RFIDManager.cpp
    ├── RFIDManager.h
    ├── WebServerManager.cpp
    ├── WebServerManager.h
    └── WebAssets.h
```

Die frühere HardwareTestMode-/Display-/IR-Struktur wurde inzwischen entfernt. Hardwaretests liegen heute als separate Build-Ziele vor.

---

## 🧪 Test-Firmwares

Das Repository enthält zusätzliche Standalone-Firmwares zur Fehlersuche:

| PlatformIO Environment | Zweck |
|---|---|
| `esp32dev_usb` | normale Firmware per USB |
| `esp32dev_ota` | normale Firmware per OTA |
| `dumpinfo_usb` | RFID-/Tag-Diagnose |
| `dfplayer_test_usb` | DFPlayer-Test |
| `dfplayer_test_ota` | DFPlayer-Test per OTA |
| `gpio_function_test_usb` | GPIO-/Bedienelementetest |
| `gpio_function_test_ota` | GPIO-Test per OTA |

Das Standardziel ist aktuell:

```ini
default_envs = esp32dev_ota
```

---

## 🚀 Build mit PlatformIO

Voraussetzungen:

- VS Code + PlatformIO oder PlatformIO Core
- USB-Verbindung für den ersten Flash
- anschließend optional OTA

Repository klonen:

```bash
git clone https://github.com/toor0001/hoeribert_3.0.git
cd hoeribert_3.0
```

Normale Firmware über USB bauen und hochladen:

```bash
pio run -e esp32dev_usb -t upload
```

Serial Monitor:

```bash
pio device monitor -b 115200
```

OTA-Build:

```bash
pio run -e esp32dev_ota -t upload
```

> [!NOTE]
> Das in `platformio.ini` eingetragene OTA-Ziel ist momentan `noxon.local`. Falls dein Gerät unter einem anderen Hostnamen erreichbar ist, muss `upload_port` entsprechend angepasst oder die IP-Adresse verwendet werden.

---

## 🔐 WLAN-Konfiguration

Die echten Zugangsdaten gehören **nicht** ins Repository.

Vorlage kopieren:

```bash
cp include/secrets.example.h include/secrets.h
```

Danach anpassen:

```cpp
#pragma once

constexpr const char* WIFI_SSID = "dein-wlan-name";
constexpr const char* WIFI_PASS = "dein-wlan-passwort";
constexpr const char* OTA_NAME = "hoeribert";
```

`include/secrets.h` ist über `.gitignore` ausgeschlossen.

---

## 📦 Abhängigkeiten

Die benötigten Bibliotheken werden von PlatformIO automatisch installiert:

- `miguelbalboa/MFRC522`
- `dfrobot/DFRobotDFPlayerMini`

Framework:

```text
Arduino on ESP32
```

Board-Konfiguration:

```text
lolin32_lite
```

---

## 🚧 Aktueller Projektstatus

Höribert 3.0 ist ein aktiv weiterentwickeltes DIY-Projekt.

### Funktioniert bereits

- ESP32 / Lolin32 Lite als Hauptcontroller
- DFPlayer-Wiedergabe
- RFID-Erkennung und TonUINO-Dekodierung
- Album-Wiedergabe aus RFID-Ordnern
- Play/Pause
- Vor / Zurück
- Lautstärkepoti
- Sleep-Timer
- Status-LED
- lokale Bookmark-Speicherung
- WLAN
- OTA
- TCP-Live-Logs
- USB-/OTA-Testfirmwares

### Noch in Arbeit / vorbereitet

- vollständiges Wiederaufnehmen gespeicherter Bookmarks
- weitere TonUINO-Modi
- Reaktivierung und Fertigstellung des Webinterfaces
- Bereinigung verbliebener `SL58`-/`noxon`-Legacy-Bezeichnungen
- Abgleich bzw. Bereinigung älterer Testdokumentation

---

## ❤️ Idee hinter dem Projekt

Höribert soll sich nicht wie ein ESP32-Projekt in einer Plastikbox anfühlen.

Die Idee ist, einen echten alten Kassettenrecorder weiterleben zu lassen: Die großen mechanischen Tasten, der Lautstärkeregler und das massive Gehäuse bleiben Teil des Erlebnisses – nur Kassette und Tonkopf werden durch RFID und digitale Audiodateien ersetzt.

**Vintage außen. ESP32 innen. Hörspiele wie früher – nur komfortabler.**
