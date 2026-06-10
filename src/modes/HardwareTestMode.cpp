#include "HardwareTestMode.h"

#include "hardware/AudioPlayer.h"
// #include "hardware/DisplayManager.h"  // Display deaktiviert

#include "hardware/WifiOtaManager.h"
#include "secrets.h"

#include <Arduino.h>
#include <math.h>

namespace {

// Volume
constexpr uint8_t VOL_PIN = 34;
constexpr int VOL_RAW_MIN = 40;
constexpr int VOL_RAW_MAX = 4050;
constexpr int VOL_MAX = 24;

int currentFolder = 1;
constexpr int MAX_FOLDER = 99;

// DisplayManager display;  // Display deaktiviert
WifiOtaManager wifiOtaManager;
AudioPlayer audioPlayer;
RFIDManager rfidManager;

int lastVolume = -1;
int filteredVolumeRaw = -1;

String tonuinoModeToString(byte mode);

void logLine(const String& msg) {
  // display.logLine(msg);  // Display deaktiviert
  Serial.println(msg);  // Fallback zu Serial
}

int readAverageRaw() {
  long sum = 0;
  constexpr int SAMPLES = 8;

  for (int i = 0; i < SAMPLES; i++) {
    sum += analogRead(VOL_PIN);
    delayMicroseconds(500);
  }

  return sum / SAMPLES;
}

int rawToVolume(int raw) {
  raw = constrain(raw, 0, 4095);

  if (raw <= VOL_RAW_MIN) return 0;
  if (raw >= VOL_RAW_MAX) return VOL_MAX;

  float normalized = static_cast<float>(raw - VOL_RAW_MIN) / static_cast<float>(VOL_RAW_MAX - VOL_RAW_MIN);
  float curved = powf(normalized, 0.75f);
  int volume = static_cast<int>(curved * VOL_MAX + 0.5f);
  return constrain(volume, 0, VOL_MAX);
}

void applyVolume() {
  int raw = readAverageRaw();

  if (filteredVolumeRaw < 0) {
    filteredVolumeRaw = raw;
  } else {
    filteredVolumeRaw = (filteredVolumeRaw * 3 + raw) / 4;
  }

  int volume = rawToVolume(filteredVolumeRaw);

  if (volume != lastVolume) {
    lastVolume = volume;

    audioPlayer.setVolume(volume);

    logLine("[VOLUME] " + String(volume) + "/" + String(VOL_MAX) +
            " RAW=" + String(raw) + " FILT=" + String(filteredVolumeRaw));
  }
}

void playCurrentFolder() {
  if (!audioPlayer.isReady()) return;

  // startet Ordner ab Datei 001
  // DFPlayer spielt danach automatisch weiter
  audioPlayer.playFolder(currentFolder);

  logLine("[DF] Folder=" + String(currentFolder));
  logLine("[DF] AutoPlay Folder");
}

void handleRFID() {
  if (!rfidManager.update()) return;

  logLine("[RFID] UID=" + rfidManager.getLastUid());
  logLine("[RFID] TYPE:");
  logLine(rfidManager.getLastCardType());

  if (rfidManager.hasLastRawData()) {
    TonuinoCardData card = rfidManager.readTonuinoCard();

    logLine("[TONUINO] RAW:");
    logLine(rfidManager.getLastRawData());

    if (card.valid) {
      logLine("[TONUINO] Karte bekannt");
      logLine("[TONUINO] Version: " + String(card.version));
      logLine("[TONUINO] Ordner: " + String(card.folder));
      logLine("[TONUINO] Modus: " + tonuinoModeToString(card.mode));
      logLine("[TONUINO] Special: " + String(card.special) + "/" + String(card.special2));

      if (card.mode != 2) {
        logLine("[TONUINO] Hinweis: NormalMode nutzt nur Album");
      }

      currentFolder = card.folder;
    } else {
      logLine("[TONUINO] Keine TonUINO-Karte");
      logLine("[TONUINO] Block 4 Cookie falsch");
    }
  }

  for (int i = 0; i < rfidManager.getDebugLineCount(); i++) {
    logLine(rfidManager.getDebugLine(i));
  }
}

String tonuinoModeToString(byte mode) {
  switch (mode) {
    case 1: return "Hoerspiel: zufaellige Datei";
    case 2: return "Album: ganzer Ordner";
    case 3: return "Party: Ordner zufaellig";
    case 4: return "Einzel: bestimmter Track";
    case 5: return "Hoerbuch: Ordner + Fortschritt";
    case 7: return "Hoerspiel von-bis";
    case 8: return "Album von-bis";
    case 9: return "Party von-bis";
    case 10: return "Hoerbuch einzeln";
    case 11: return "Letzte Karte wiederholen";
    default: return "Unbekannter Modus";
  }
}

} // namespace

void HardwareTestMode::begin() {
  analogReadResolution(12);
  analogSetPinAttenuation(VOL_PIN, ADC_11db);

  rfidManager.begin();

  // display.begin();  // Display deaktiviert
  wifiOtaManager.begin(WIFI_SSID, WIFI_PASS, OTA_NAME, nullptr);  // Display deaktiviert

  // display.showHardwareTestScreen();  // Display deaktiviert

  if (audioPlayer.begin()) {
    logLine(audioPlayer.getStatusText());
    int initialVolume = rawToVolume(readAverageRaw());
    lastVolume = initialVolume;
    audioPlayer.setVolume(initialVolume);

    logLine("[VOLUME] Start " + String(initialVolume) + "/" + String(VOL_MAX));
  } else {
    logLine(audioPlayer.getStatusText());
  }

  logLine("RC522 bereit.");
}

void HardwareTestMode::update() {
  wifiOtaManager.update();
  audioPlayer.update();
  applyVolume();
  handleRFID();

  delay(20);
}
