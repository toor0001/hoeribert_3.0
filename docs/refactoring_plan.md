# Refactoring-Plan NOXON Button Board

Stand: 2026-05-19

## Zielbild

Das Projekt wird schrittweise von einem funktionierenden Hardwaretest in eine modulare ESP32/PlatformIO-Anwendung überführt. Die aktuelle Hardwaretest-Funktionalität bleibt dabei zunächst die Referenz:

- OTA bleibt in jedem Modus aktiv.
- TFT-Logs bleiben als Debug- und Fehlerausgabe erhalten.
- Button-, IR-, Volume-, RFID- und DFPlayer-Tests bleiben im HardwareTestMode erhalten.
- `loop()` in `main.cpp` wird klein und delegiert nur noch an `App::update()`.
- TonUINO-Kartenlogik wird erst nach stabiler Grundstruktur ergänzt.

## Aktueller lokaler Zustand

Die gesamte Logik liegt aktuell in `src/main.cpp`.

### Pinbelegung

Aus `src/main.cpp` und `test/DOKU`:

| Funktion | Pins |
| --- | --- |
| 74HC165 Buttons | DATA GPIO19, CLOCK GPIO18, LOAD GPIO5 |
| WLAN/OTA | SSID/PASS konstant in `main.cpp`, OTA Hostname `noxon` |
| IR | GPIO27 |
| Volume-Poti | GPIO34, ADC 12 bit, `ADC_11db` |
| DFPlayer | RX GPIO16, TX GPIO17, `HardwareSerial(2)` |
| RC522 RFID | SS GPIO21, RST GPIO22, SCK GPIO14, MISO GPIO23, MOSI GPIO13 |
| TFT ILI9341 | CS GPIO25, DC GPIO26, RST GPIO33, MOSI GPIO13, SCK GPIO14 |

Wichtig: TFT und RC522 teilen sich SPI MOSI/SCK. Die Initialisierung darf deshalb nicht unnötig verändert werden.

### Vorhandene Hardwaretests

- TFT-Logging über `logLine()` mit automatischem Screen-Clear.
- Buttonscan über zwei 74HC165-Bytes in `read165()`.
- Button-Mapping A bis N mit festen Bitmasken.
- DFPlayer-Test über Buttons:
  - F: Stop
  - G: Previous
  - H: Pause/Play
  - I: Next
  - J: Status lesen
  - N: vorheriger Ordner
  - L: nächster Ordner
- Volume-Poti liest Mittelwert und setzt DFPlayer-Volume nur bei Änderung.
- IR-Ausgabe zeigt Protokoll, Command und Raw-Wert.
- RFID liest UID, PICC-Typ und versucht TonUINO-Daten zu lesen.
- MIFARE Classic liest aktuell Block 4 mit Default-Key `FF FF FF FF FF FF`.
- MIFARE Ultralight macht aktuell nur einen Dump und gibt noch keine TonUINO-Daten zurück.

### OTA-Setup

`setupOTA()` verbindet WLAN blockierend bis maximal 15 Sekunden, setzt den Hostnamen `noxon`, registriert OTA-Callbacks und ruft `ArduinoOTA.begin()`. Im aktuellen `loop()` wird `ArduinoOTA.handle()` in jedem Durchlauf aufgerufen.

Risiko: Bei der Modularisierung darf `ArduinoOTA.handle()` nicht nur im NormalMode oder nur im HardwareTestMode liegen. Besser ist eine zentrale `WifiOtaManager::update()`-Methode, die von `App::update()` in jedem Modus zuerst aufgerufen wird.

## TonUINO-Referenzanalyse

Referenz: <https://github.com/xfjx/TonUINO>

Relevante Erkenntnisse aus `Tonuino.ino`:

- Die alte Struktur `nfcTagObject` speichert Cookie, Version, Folder, Mode und Special.
- Das Cookie ist `0x13 0x37 0xB3 0x47`, numerisch `322417479`.
- Bei Classic-Karten wird Sektor 1, Block 4 verwendet.
- Gelesen wird mit MFRC522 aus Block 4 in einen 16-Byte-Puffer.
- Die Bytes werden so interpretiert:
  - Byte 0..3: Magic Cookie
  - Byte 4: Version
  - Byte 5: Folder
  - Byte 6: Mode
  - Byte 7: Special
- Spätere/2.1-nahe TonUINO-Strukturen verwenden `folderSettings` mit `folder`, `mode`, `special`, `special2`; deshalb sollte unser neues Format Byte 8 als `special2` mitlesen.

Geplante neue Datenstruktur:

```cpp
struct TonuinoCardData {
  bool valid;
  uint8_t version;
  uint8_t folder;
  uint8_t mode;
  uint8_t special;
  uint8_t special2;
  String uid;
  String cardType;
};
```

Für den späteren RFIDManager:

- MIFARE Classic: mit Default-Key `FF FF FF FF FF FF` gegen Trailer Block 7 authentifizieren und danach Datenblock 4 lesen, wie im originalen TonUINO.
- MIFARE Ultralight/NTAG: Pages 8 bis 11 lesen und zu 16 Bytes zusammensetzen.
- Nur Lesen und Dekodieren übernehmen.
- Keine Anlern-Funktion, kein Kartenlöschen, kein Admin-Menü.
- Keine Übernahme der alten TonUINO-Architektur.
- Projektannahme: Die SD-Karte ist final vorbereitet und alle vorhandenen Karten sind bereits angelernt/gemapped. Deshalb werden nur vorhandene Karten gelesen.
- Projektannahme: Alle Karten sind auf Album-Modus eingerichtet. Andere TonUINO-Modi werden im NormalMode nicht umgesetzt.

## Zielstruktur

```text
src/main.cpp
src/App.cpp
src/App.h

src/modes/NormalMode.cpp
src/modes/NormalMode.h
src/modes/HardwareTestMode.cpp
src/modes/HardwareTestMode.h

src/hardware/RFIDManager.cpp
src/hardware/RFIDManager.h
src/hardware/AudioPlayer.cpp
src/hardware/AudioPlayer.h
src/hardware/ButtonBoard.cpp
src/hardware/ButtonBoard.h
src/hardware/DisplayManager.cpp
src/hardware/DisplayManager.h
src/hardware/IRManager.cpp
src/hardware/IRManager.h
src/hardware/WifiOtaManager.cpp
src/hardware/WifiOtaManager.h
```

## Refactoring-Strategie

### Schritt 1: Grundstruktur ohne Funktionsverlust

Ziel: Noch keine TonUINO-Neuimplementierung, nur das Programmgerüst schaffen.

Änderungen:

- `App` einführen.
- `main.cpp` auf `setup()` und `loop()` reduzieren:
  - `app.begin()`
  - `app.update()`
- Mode-Auswahl beim Boot einführen.
- `BTN_J` als Boot-Testtaste verwenden.
- Wenn die Taste beim Boot gehalten ist: `HardwareTestMode`.
- Wenn nicht: `NormalMode`.
- Für diesen ersten Umbau darf `NormalMode` noch minimal sein und nur Status anzeigen.
- `HardwareTestMode` übernimmt den bisherigen funktionierenden Ablauf.
- OTA-Update zentral sicherstellen.

Schon in Schritt 1 sollte gelten:

- `ArduinoOTA.handle()` bzw. später `WifiOtaManager::update()` läuft unabhängig vom aktiven Modus.
- Keine langen neuen `delay()`-Blöcke einbauen.
- Bestehende Initialisierungsreihenfolge möglichst erhalten.

Empfohlener kleiner Commit:

```text
Introduce App skeleton and boot mode selection
```

### Schritt 2: Hardware-Abstraktionen extrahieren

Ziel: Funktionierende Hardwaretests in Klassen verschieben, aber Verhalten gleich lassen.

Reihenfolge:

1. `DisplayManager`: `begin()`, `showBootScreen()`, `showHardwareTestScreen()`, `logLine()`, `showError()`.
2. `ButtonBoard`: Pin-Setup, `read165()`, `update()`, Press/Hold-Erkennung, Buttonnamen.
3. `IRManager`: IR-Empfänger initialisieren, dekodierte Protokoll-/Command-/Raw-Daten bereitstellen.
4. `WifiOtaManager`: WLAN-Verbindung, OTA-Callbacks, `update()`, Status-/IP-Text.
5. `AudioPlayer`: DFPlayer-Initialisierung, Volume, Folder/Track-Steuerung, Status.
6. `RFIDManager`: RC522-Initialisierung, UID/Card-Type/Raw-Daten lesen.

Jede Extraktion sollte einzeln baubar bleiben.

Empfohlene kleine Commits:

```text
Extract DisplayManager from hardware test
Extract ButtonBoard scan and button mapping
Extract IRManager from hardware test
Extract WifiOtaManager while preserving OTA callbacks
Extract AudioPlayer DFPlayer wrapper
Extract RFIDManager basic UID and raw reads
```

### Schritt 3: HardwareTestMode bereinigen

Ziel: Der HardwareTestMode nutzt nur noch die Manager.

Er soll weiterhin:

- Buttonnamen auf TFT loggen.
- IR-Daten über `IRManager` loggen.
- Volume-Änderungen loggen und DFPlayer-Volume setzen.
- DFPlayer-Testbuttons behalten.
- RFID UID, Typ, Rohdaten und TonUINO-Basisdaten anzeigen.
- Fehler auf TFT und Serial ausgeben.
- OTA aktiv lassen.

Empfohlener kleiner Commit:

```text
Move hardware test loop into HardwareTestMode
```

### Schritt 4: TonUINO-Kartenlesen im RFIDManager

Erst nach stabiler Grundstruktur.

Implementieren:

- `TonuinoCardData`.
- `RFIDManager::readTonuinoCard()`.
- Classic Block 4 mit Default-Key.
- Ultralight/NTAG Pages 8 bis 11 zu 16 Bytes.
- `getLastUid()`, `getLastRawData()`, `getLastError()`.
- Validierung des Cookies `13 37 B3 47`.
- Version, Folder, Mode, Special, Special2 aus Bytes 4 bis 8.

Nicht implementieren:

- Karten schreiben.
- Karten resetten.
- Admin-Menü.
- Anlernmodus.

Empfohlener kleiner Commit:

```text
Add TonUINO card decoding to RFIDManager
```

### Schritt 5: NormalMode MVP

Ziel: Erste nutzbare RFID-Audioplayer-Schleife.

Initial minimal:

- RFID-Karte lesen.
- Validierte TonUINO-Karte anzeigen.
- Ordner aus Karte übernehmen.
- Nur Modus 2 unterstützen: Album/Ordner ab Track 1. Andere Modi werden angezeigt, aber nicht abgespielt.
- Tasten Play/Pause, Next, Previous, Volume verwenden.
- `BTN_B` toggelt im NormalMode die Anzeige. Aktuell ist nur ein schwarzer Screen/Status-Neuzeichnen möglich, weil die TFT-LED laut Doku fest an 3.3V liegt.
- Unbekannte Modi nur anzeigen, noch nicht vollständig ausführen.

Später:

- Mode 1: zufälliger Track.
- Mode 3: zufälliger Ordner/Party.
- Mode 4: Einzeltrack aus `special`.
- Mode 5/10: Hörbuchlogik mit Fortschritt nur bewusst planen, Speicherort klären.
- Von-bis-Modi 7 bis 9 mit `special`/`special2`.

Empfohlener kleiner Commit:

```text
Add NormalMode RFID folder playback MVP
```

## Offene Entscheidungen vor Code-Umbau

- Boot-Testtaste ist festgelegt: `BTN_J`. Wenn `BTN_J` beim Einschalten gehalten wird, startet `HardwareTestMode`; sonst startet `NormalMode`.
- WLAN-Zugangsdaten mittelfristig aus `main.cpp` entfernen, aber im ersten Schritt unverändert lassen, um OTA nicht zu gefährden.
- IR wird als eigener `IRManager` in die Hardware-Schicht aufgenommen, weil die Erkennung ein eigenständiger Hardwaretest und später optional eine Steuerquelle sein kann.
- Display-Beleuchtung später hardwareseitig über einen freien GPIO verdrahten. Dann `DisplayManager` um echtes Backlight-On/Off erweitern und `BTN_J` im NormalMode diesen GPIO schalten lassen.
- Buzzer später hardwareseitig über einen freien GPIO einbauen. Er soll im NormalMode das erfolgreiche Erkennen einer neuen Karte kurz quittieren.
- WLAN und damit OTA sollen später nur im Testmode/HardwareTestMode aktiv sein. Im NormalMode soll WLAN deaktiviert bleiben, damit der Player schneller und stromsparender startet.
- `delay(1000)` im aktuellen Setup und `delay(500)` für DFPlayer sind bestehendes Verhalten. Bei späterer Stabilisierung können sie reduziert oder durch nicht-blockierende Zustände ersetzt werden.
- Der aktuelle WLAN-Connect ist bis zu 15 Sekunden blockierend. Für den ersten Umbau erhalten, später optional nicht-blockierend machen.

## Erster konkreter Implementierungsschritt

Noch keinen großen Code-Umbau.

Minimaler Umfang:

1. `App.h/.cpp` anlegen.
2. `modes/HardwareTestMode.h/.cpp` anlegen.
3. `modes/NormalMode.h/.cpp` anlegen.
4. Bestehenden Code zunächst möglichst mechanisch verschieben.
5. `main.cpp` klein halten.
6. OTA-Aufruf zentral in `App::update()` oder vor jedem Mode-Update sicherstellen.
7. Nach dem Umbau `pio run` ausführen.

Erwartete Prüfung nach Schritt 1:

- Firmware baut.
- TFT zeigt Boot-/Hardwaretest-Text.
- OTA ist erreichbar.
- Buttonausgaben erscheinen weiter.
- DFPlayer-Testbuttons funktionieren weiter.
- RFID liest weiter UID und Classic Block 4.
- IR-Ausgabe bleibt erhalten.

## Nicht-Ziele für Schritt 1

- Keine vollständige TonUINO-Mode-Engine.
- Keine neue Karten-Schreiblogik.
- Keine Admin-/Anlern-/Löschfunktionen.
- Keine große Umbenennung von Pins oder Buttonmasken.
- Keine Änderung der Verkabelungsannahmen.
