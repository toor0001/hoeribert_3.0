#include "NormalMode.h"

#include "hardware/AudioPlayer.h"
#include "hardware/RFIDManager.h"
#include "hardware/WebServerManager.h"
#include "hardware/HardwarePins.h"
#include "hardware/PowerManager.h"
#include "secrets.h"

#include <Arduino.h>
#include <Preferences.h>

namespace {

constexpr uint8_t VOL_PIN = HardwarePins::VOLUME;
constexpr int VOL_RAW_MIN = 40;
constexpr int VOL_RAW_MAX = 4050;
constexpr int VOL_MAX = 10;
constexpr unsigned long DF_VOLUME_COMMAND_SETTLE_MS = 120;
constexpr unsigned long VOL_POLL_MS = 120;
constexpr unsigned long VOL_STEP_STABLE_MS = 220;
constexpr int VOL_RAW_LOG_DELTA = 80;
constexpr unsigned long VOL_RAW_LOG_MS = 700;
constexpr int VOL_RAIL_LOW = 25;
constexpr int VOL_RAIL_HIGH = 4070;
constexpr int VOL_GLITCH_JUMP = 900;
constexpr unsigned long VOL_RAIL_STABLE_MS = 350;
constexpr uint8_t PLAY_BUTTON_PIN = HardwarePins::PLAY_BUTTON;
constexpr unsigned long PLAY_BUTTON_DEBOUNCE_MS = 50;
constexpr uint8_t BTN_FROWARD_PIN = HardwarePins::FORWARD_BUTTON;
constexpr uint8_t BTN_BACK_PIN = HardwarePins::BACK_BUTTON;
constexpr unsigned long TRACK_BUTTON_DEBOUNCE_MS = 20;
constexpr uint8_t TIMER_BUTTON_PIN = HardwarePins::TIMER_BUTTON;
constexpr unsigned long TIMER_BUTTON_DEBOUNCE_MS = 50;
constexpr unsigned long TIMER_BUTTON_LONG_PRESS_MS = 1200;
constexpr uint8_t TIMER_BUTTON_STEP_MINUTES = 10;
constexpr uint8_t STATUS_LED_PIN = HardwarePins::STATUS_LED;
constexpr unsigned long STATUS_LED_BLINK_MS = 80;
constexpr unsigned long RFID_POLL_MS = 50;
constexpr unsigned long INACTIVITY_SLEEP_MS = 10UL * 60UL * 1000UL;

const char* folderTitle(uint8_t folder) {
  switch (folder) {
      case 1: return "Der Super-Papagei";
      case 2: return "Der Phantomsee";
      case 3: return "Der Karpatenhund";
      case 4: return "Die schwarze Katze";
      case 5: return "Der Fluch des Rubins";
      case 6: return "Der sprechende Totenkopf";
      case 7: return "Der unheimliche Drache";
      case 8: return "Der grüne Geist";
      case 9: return "Die rätselhaften Bilder";
      case 10: return "Und die flüsternde Mumie";
      case 11: return "Und das Gespensterschloss";
      case 12: return "Und der seltsame Wecker";
      case 13: return "Und der lachende Schatten";
      case 14: return "Und der schreiende Nebel";
      case 15: return "Und der rasende Löwe";
      case 16: return "Und der Zauberspiegel";
      case 17: return "Und die gefährliche Erbschaft";
      case 18: return "Und die Geisterinsel";
      case 19: return "Und der Teufelsberg";
      case 20: return "Und die flammende Spur";
      case 21: return "Und der tanzende Teufel";
      case 22: return "Und der verschwundene Schatz";
      case 23: return "Und das Aztekenschwert";
      case 24: return "Und die silberne Spinne";
      case 25: return "Und die singende Schlange";
      case 26: return "Und die Silbermine";
      case 27: return "Und der magische Kreis";
      case 28: return "Und der Doppelgänger";
      case 29: return "Und die falschen Detektive";
      case 30: return "Und das Riff der Haie";
      case 31: return "Und das Narbengesicht";
      case 32: return "Und der Ameisenmensch";
      case 33: return "Und die bedrohte Ranch";
      case 34: return "Und der rote Pirat";
      case 35: return "Und der Höhlenmensch";
      case 36: return "Und der Super-Wal";
      case 37: return "Und der heimliche Hehler";
      case 38: return "Und der unsichtbare Gegner";
      case 39: return "Und die Perlenvögel";
      case 40: return "Und der Automarder";
      case 41: return "Und das Volk der Winde";
      case 42: return "Und der weinende Sarg";
      case 43: return "Und der höllische Werwolf";
      case 44: return "Und der gestohlene Preis";
      case 45: return "Und das Gold der Wikinger";
      case 46: return "Und der schrullige Millionär";
      case 47: return "Und der giftige Gockel";
      case 48: return "Und die gefährlichen Fässer";
      case 49: return "Und die Comic-Diebe";
      case 50: return "Und der verschwundene Filmstar";
      case 51: return "Und der riskante Ritt";
      case 52: return "Und die Musikpiraten";
      case 53: return "Und die Automafia";
      case 54: return "Und der rote Rächer";
      case 55: return "Und der verrückte Maler";
      case 56: return "Und der verschwundene Fußballer";
      case 57: return "Tatort Zirkus";
      case 58: return "Und der verrückte Erfinder";
      case 59: return "Und die Rache des Tigers";
      case 60: return "Und die Geisterbahn";
      case 61: return "Und die Rache des Untoten";
      case 62: return "Spuk im Hotel";
      case 63: return "Fußball-Gangster";
      case 64: return "Geisterstadt";
      case 65: return "Diamantenschmuggel";
      case 66: return "Die Schattenmänner";
      case 67: return "Das Geheimnis der Särge";
      case 68: return "Schatz im Bergsee";
      case 69: return "Späte Rache";
      case 70: return "Schüsse aus dem Dunkel";
      case 71: return "Die verschwundene Seglerin";
      case 72: return "Dreckiger Deal";
      case 73: return "Poltergeist";
      case 74: return "Das brennende Schwert";
      case 75: return "Die Spur des Raben";
      case 76: return "Stimmen aus dem Nichts";
      case 77: return "Pistenteufel";
      case 78: return "Das leere Grab";
      case 79: return "Im Bann des Voodoo";
      case 80: return "Geheimnis der Karten";
      case 81: return "Verdeckte Fouls";
      case 82: return "Die Karten des Bösen";
      case 83: return "Meuterei auf hoher See";
      case 84: return "Musik des Teufels";
      case 85: return "Feuerturm";
      case 86: return "Nacht in Angst";
      case 87: return "Wolfsgesicht";
      case 88: return "Vampir im Internet";
      case 89: return "Tödliche Spur";
      case 90: return "Der Feuerteufel";
      case 91: return "Labyrinth der Götter";
      case 92: return "Todesflug";
      case 93: return "Das schwarze Monster";
      case 94: return "Botschaft von Geisterhand";
      case 95: return "Rufmord";
      case 96: return "Das rote Phantom";
      case 97: return "Insektenstachel";
      case 98: return "Tal des Schreckens";
      case 99: return "Ruf der Krähen";
      default: return "Unbekannte Folge";
  }
}

RFIDManager rfidManager;
AudioPlayer audioPlayer;
WebServerManager webServer;
PowerManager powerManager;
Preferences bookmarkPrefs;

uint8_t currentFolder = 0;
int lastVolume = -1;
int lastDfVolume = -1;
int filteredVolumeRaw = -1;
int pendingVolume = -1;
int pendingDfVolume = -1;
int lastVolumeRawLogged = -1;
int volumeRawCandidate = -1;
int lastAcceptedVolumeRaw = -1;
unsigned long pendingVolumeChangedAt = 0;
unsigned long lastVolumePollAt = 0;
unsigned long lastVolumeRawLoggedAt = 0;
unsigned long volumeRawCandidateSince = 0;
unsigned long lastRfidPollAt = 0;
unsigned long lastRelevantActivityAt = 0;
String activeCardUid = "";
bool activeBookmarkValid = false;
uint8_t activeBookmarkTrack = 0;
uint16_t activeBookmarkSeconds = 0;
bool waitingForPlayButton = false;
uint8_t pendingStartTrack = 0;
uint16_t pendingStartSeconds = 0;
bool pendingStartHasBookmark = false;
bool playbackPausedByButton = false;
bool playButtonRawState = false;
bool playButtonState = false;
bool lastPlayButtonState = false;
unsigned long playButtonRawChangedAt = 0;
bool btnForwardRawState = false;
bool btnForwardState = false;
bool lastBtnForwardState = false;
unsigned long btnForwardRawChangedAt = 0;
bool btnBackRawState = false;
bool btnBackState = false;
bool lastBtnBackState = false;
unsigned long btnBackRawChangedAt = 0;
bool timerButtonRawState = false;
bool timerButtonState = false;
bool lastTimerButtonState = false;
bool timerButtonArmed = true;
unsigned long timerButtonRawChangedAt = 0;
unsigned long timerButtonPressedAt = 0;
bool statusLedOn = false;
unsigned long statusLedOffAt = 0;
unsigned long sleepTimerEndsAt = 0;
unsigned long lastSleepTimerDrawnSecond = 0;
unsigned long notificationEndsAt = 0;
String lastDebugCardUid = "";
unsigned long lastDebugCardLoggedAt = 0;
unsigned long lastRfidNoCardLoggedAt = 0;
bool dfPlayerUnavailableLogged = false;
bool initialDfVolumeApplied = false;
uint8_t playStartCount = 0;
uint8_t lastTonuinoFolder = 0;
uint8_t lastTonuinoMode = 0;
constexpr unsigned long NOTIFICATION_MS = 3000;
constexpr unsigned long RFID_DEBUG_REPEAT_MS = 2000;

int readMedianRaw() {
  constexpr int SAMPLES = 7;
  int samples[SAMPLES];

  for (int i = 0; i < SAMPLES; i++) {
    samples[i] = analogRead(VOL_PIN);
    delayMicroseconds(300);
  }

  for (int i = 1; i < SAMPLES; i++) {
    int value = samples[i];
    int j = i - 1;
    while (j >= 0 && samples[j] > value) {
      samples[j + 1] = samples[j];
      j--;
    }
    samples[j + 1] = value;
  }

  return samples[SAMPLES / 2];
}

int stabilizeVolumeRaw(int raw, unsigned long now) {
  if (lastAcceptedVolumeRaw < 0) {
    lastAcceptedVolumeRaw = raw;
    volumeRawCandidate = raw;
    volumeRawCandidateSince = now;
    return raw;
  }

  bool rawAtRail = raw <= VOL_RAIL_LOW || raw >= VOL_RAIL_HIGH;
  bool acceptedAtRail = lastAcceptedVolumeRaw <= VOL_RAIL_LOW || lastAcceptedVolumeRaw >= VOL_RAIL_HIGH;
  bool suspiciousRailJump = rawAtRail && !acceptedAtRail && abs(raw - lastAcceptedVolumeRaw) >= VOL_GLITCH_JUMP;

  if (!suspiciousRailJump) {
    lastAcceptedVolumeRaw = raw;
    volumeRawCandidate = raw;
    volumeRawCandidateSince = now;
    return raw;
  }

  if (abs(raw - volumeRawCandidate) > VOL_RAW_LOG_DELTA) {
    volumeRawCandidate = raw;
    volumeRawCandidateSince = now;
    return lastAcceptedVolumeRaw;
  }

  if (static_cast<unsigned long>(now - volumeRawCandidateSince) >= VOL_RAIL_STABLE_MS) {
    lastAcceptedVolumeRaw = raw;
    return raw;
  }

  return lastAcceptedVolumeRaw;
}

int rawToVolume(int raw) {
  raw = constrain(raw, VOL_RAW_MIN, VOL_RAW_MAX);
  return map(raw, VOL_RAW_MIN, VOL_RAW_MAX, VOL_MAX, 0);
}

int rawToDfVolume(int raw) {
  if (raw >= 4070) return 0;
  if (raw >= 3950) return 1;
  if (raw >= 3750) return 2;
  if (raw >= 3500) return 3;
  if (raw >= 3300) return 4;
  if (raw >= 3000) return 6;
  if (raw >= 2700) return 8;
  if (raw >= 2400) return 10;
  if (raw >= 2100) return 12;
  if (raw >= 1700) return 14;
  if (raw >= 1200) return 16;
  if (raw >= 700) return 18;
  if (raw >= 300) return 19;
  return 20;
}

void logWeb(const String& text);

void noteRelevantActivity() {
  lastRelevantActivityAt = millis();
}

void applyVolume() {
  unsigned long now = millis();
  if (static_cast<unsigned long>(now - lastVolumePollAt) < VOL_POLL_MS) {
    return;
  }
  lastVolumePollAt = now;

  int raw = readMedianRaw();
  int stableRaw = stabilizeVolumeRaw(raw, now);

  filteredVolumeRaw = stableRaw;

  int volume = rawToVolume(filteredVolumeRaw);
  int dfVolume = rawToDfVolume(filteredVolumeRaw);
  bool rawMoved = lastVolumeRawLogged < 0 ||
                  abs(filteredVolumeRaw - lastVolumeRawLogged) >= VOL_RAW_LOG_DELTA;
  bool rawLogReady = static_cast<unsigned long>(now - lastVolumeRawLoggedAt) >= VOL_RAW_LOG_MS;
  if (rawMoved && rawLogReady) {
    lastVolumeRawLogged = filteredVolumeRaw;
    lastVolumeRawLoggedAt = now;
    String potiLog = "[POTI] RAW=" + String(raw) +
                     " STABLE=" + String(stableRaw) +
                     " FILT=" + String(filteredVolumeRaw) +
                     " -> logical " + String(volume) + "/10";
    Serial.println(potiLog);
    logWeb(potiLog);
  }

  if (volume == lastVolume && dfVolume == lastDfVolume) {
    pendingVolume = volume;
    pendingDfVolume = dfVolume;
    pendingVolumeChangedAt = now;
    return;
  }

  if (volume != pendingVolume || dfVolume != pendingDfVolume) {
    pendingVolume = volume;
    pendingDfVolume = dfVolume;
    pendingVolumeChangedAt = now;
  }

  bool largeChange = lastDfVolume < 0 || abs(dfVolume - lastDfVolume) >= 4;
  bool stableStep = static_cast<unsigned long>(now - pendingVolumeChangedAt) >= VOL_STEP_STABLE_MS;
  if (!largeChange && !stableStep) {
    return;
  }

  lastVolume = volume;
  lastDfVolume = dfVolume;
  audioPlayer.setVolume(volume, dfVolume, filteredVolumeRaw);
  noteRelevantActivity();
  String volumeLog = "[VOLUME] RAW=" + String(filteredVolumeRaw) +
                     " -> DF " + String(audioPlayer.getMappedDfVolume()) +
                     "/30 (direkt aus RAW, logical " + String(volume) + "/10)";
  Serial.println(volumeLog);
  logWeb(volumeLog);
}

void logCurrentFolder() {
  Serial.println("[NORMAL] Ordner: " + String(currentFolder) + " - " + folderTitle(currentFolder));
}

void blinkStatusLed() {
  digitalWrite(STATUS_LED_PIN, HIGH);
  statusLedOn = true;
  statusLedOffAt = millis() + STATUS_LED_BLINK_MS;
}

void updateStatusLed() {
  if (!statusLedOn) {
    return;
  }

  if (static_cast<long>(statusLedOffAt - millis()) > 0) {
    return;
  }

  digitalWrite(STATUS_LED_PIN, LOW);
  statusLedOn = false;
}

void logWeb(const String& text) {
  webServer.log(text);
}

void logRfidDebugDetails() {
  if (rfidManager.hasLastRawData()) {
    logWeb("[RFID] RAW " + rfidManager.getLastRawData());
  }

  String error = rfidManager.getLastError();
  if (error.length() > 0) {
    logWeb("[RFID] Fehler " + error);
  }

  for (int i = 0; i < rfidManager.getDebugLineCount(); i++) {
    logWeb(rfidManager.getDebugLine(i));
  }
}

void logNormalState() {
  if (currentFolder > 0) {
    logCurrentFolder();
  } else {
    Serial.println("[NORMAL] Idle - bereit für Karte");
  }

  if (sleepTimerEndsAt > 0) {
    unsigned long remainingSeconds = max(1UL, (sleepTimerEndsAt - millis() + 999) / 1000);
    Serial.println("[NORMAL] Sleep Timer: " + String(remainingSeconds) + "s");
  }
}

bool addSleepTimerMinutes(uint8_t minutes) {
  if (!audioPlayer.isPlayingNow()) {
    Serial.println("[NORMAL] Sleep Timer ignoriert: keine Wiedergabe");
    logWeb("[TIMER] + " + String(minutes) + " min ignoriert: keine Wiedergabe");
    return false;
  }

  unsigned long addMs = static_cast<unsigned long>(minutes) * 60UL * 1000UL;

  if (sleepTimerEndsAt == 0 || static_cast<long>(sleepTimerEndsAt - millis()) <= 0) {
    sleepTimerEndsAt = millis() + addMs;
  } else {
    sleepTimerEndsAt += addMs;
  }

  lastSleepTimerDrawnSecond = 0;
  unsigned long remainingSeconds = max(1UL, (sleepTimerEndsAt - millis() + 999) / 1000);
  Serial.println("[NORMAL] Sleep Timer +" + String(minutes) + " min");
  logWeb("[TIMER] +" + String(minutes) + " min -> " + String(remainingSeconds) + "s verbleibend");
  return true;
}

void clearSleepTimer() {
  bool wasActive = sleepTimerEndsAt > 0;
  sleepTimerEndsAt = 0;
  lastSleepTimerDrawnSecond = 0;
  Serial.println("[NORMAL] Sleep Timer " + String(wasActive ? "geloescht" : "war nicht aktiv"));
  logWeb("[TIMER] " + String(wasActive ? "geloescht" : "war nicht aktiv"));
}

String formatTrack(uint8_t track) {
  String text = "";
  if (track < 10) text += "00";
  else if (track < 100) text += "0";
  text += String(track);
  return text;
}

String formatTime(uint16_t seconds) {
  uint16_t minutes = seconds / 60;
  uint8_t restSeconds = seconds % 60;
  String text = String(minutes) + ":";
  if (restSeconds < 10) text += "0";
  text += String(restSeconds);
  return text;
}

void showTemporaryNotification(const String& title, const String& detail = "") {
  notificationEndsAt = millis() + NOTIFICATION_MS;
  Serial.println("[NORMAL] Notification: " + title + (detail.length() > 0 ? " - " + detail : ""));
}

void showBookmarkError(const String& detail) {
  showTemporaryNotification("FEHLER", detail.length() > 0 ? detail : "Bookmark");
}

bool notificationActive() {
  return notificationEndsAt > 0 && static_cast<long>(notificationEndsAt - millis()) > 0;
}

void updateTemporaryNotification() {
  if (notificationEndsAt == 0) {
    return;
  }

  if (static_cast<long>(notificationEndsAt - millis()) > 0) {
    return;
  }

  notificationEndsAt = 0;
  logNormalState();
}

void rememberDisplayedBookmark(bool valid, uint8_t track, uint16_t seconds) {
  activeBookmarkValid = valid;
  activeBookmarkTrack = valid ? track : 0;
  activeBookmarkSeconds = valid ? seconds : 0;
}

bool writeCurrentBookmark(const char* reason);

void updatePlayButton() {
  bool rawState = digitalRead(PLAY_BUTTON_PIN) == LOW;
  unsigned long now = millis();

  if (rawState != playButtonRawState) {
    playButtonRawState = rawState;
    playButtonRawChangedAt = now;
  }

  if (rawState != playButtonState &&
      static_cast<unsigned long>(now - playButtonRawChangedAt) >= PLAY_BUTTON_DEBOUNCE_MS) {
    playButtonState = rawState;
    noteRelevantActivity();
  }
}

void updateBtnForward() {
  bool rawState = digitalRead(BTN_FROWARD_PIN) == LOW;
  unsigned long now = millis();

  if (rawState != btnForwardRawState) {
    btnForwardRawState = rawState;
    btnForwardRawChangedAt = now;
  }

  if (rawState != btnForwardState &&
      static_cast<unsigned long>(now - btnForwardRawChangedAt) >= TRACK_BUTTON_DEBOUNCE_MS) {
    btnForwardState = rawState;
  }

  if (btnForwardState == lastBtnForwardState) {
    return;
  }

  lastBtnForwardState = btnForwardState;
  String stateText = btnForwardState ? "gemeldet" : "frei";
  Serial.println("[BTN_FROWARD] GPIO" + String(BTN_FROWARD_PIN) + " " + stateText);
  logWeb("[BTN_FROWARD] GPIO" + String(BTN_FROWARD_PIN) + " " + stateText);
  if (btnForwardState) {
    noteRelevantActivity();
    if (audioPlayer.isPlayingNow()) {
      audioPlayer.next();
      Serial.println("[NORMAL] BTN_FROWARD GPIO" + String(BTN_FROWARD_PIN) + " GND -> naechster Titel");
      logWeb("[BTN_FROWARD] GPIO" + String(BTN_FROWARD_PIN) + " GND -> naechster Titel");
    } else {
      Serial.println("[NORMAL] BTN_FROWARD GPIO" + String(BTN_FROWARD_PIN) + " GND ignoriert: keine Wiedergabe");
      logWeb("[BTN_FROWARD] GPIO" + String(BTN_FROWARD_PIN) + " GND ignoriert: keine Wiedergabe");
    }
    blinkStatusLed();
  }
}

void updateBtnBack() {
  bool rawState = digitalRead(BTN_BACK_PIN) == LOW;
  unsigned long now = millis();

  if (rawState != btnBackRawState) {
    btnBackRawState = rawState;
    btnBackRawChangedAt = now;
  }

  if (rawState != btnBackState &&
      static_cast<unsigned long>(now - btnBackRawChangedAt) >= TRACK_BUTTON_DEBOUNCE_MS) {
    btnBackState = rawState;
  }

  if (btnBackState == lastBtnBackState) {
    return;
  }

  lastBtnBackState = btnBackState;
  String stateText = btnBackState ? "gemeldet" : "frei";
  Serial.println("[BTN_BACK] GPIO" + String(BTN_BACK_PIN) + " " + stateText);
  logWeb("[BTN_BACK] GPIO" + String(BTN_BACK_PIN) + " " + stateText);
  if (btnBackState) {
    noteRelevantActivity();
    if (audioPlayer.isPlayingNow()) {
      audioPlayer.previous();
      Serial.println("[NORMAL] BTN_BACK GPIO" + String(BTN_BACK_PIN) + " GND -> vorheriger Titel");
      logWeb("[BTN_BACK] GPIO" + String(BTN_BACK_PIN) + " GND -> vorheriger Titel");
    } else {
      Serial.println("[NORMAL] BTN_BACK GPIO" + String(BTN_BACK_PIN) + " GND ignoriert: keine Wiedergabe");
      logWeb("[BTN_BACK] GPIO" + String(BTN_BACK_PIN) + " GND ignoriert: keine Wiedergabe");
    }
    blinkStatusLed();
  }
}

void updateTimerButton() {
  bool rawState = digitalRead(TIMER_BUTTON_PIN) == LOW;
  unsigned long now = millis();

  if (rawState != timerButtonRawState) {
    timerButtonRawState = rawState;
    timerButtonRawChangedAt = now;
  }

  if (rawState != timerButtonState &&
      static_cast<unsigned long>(now - timerButtonRawChangedAt) >= TIMER_BUTTON_DEBOUNCE_MS) {
    timerButtonState = rawState;
  }

  // A button held during boot selected maintenance mode. Ignore that entire
  // press and arm the normal sleep-timer action only after a stable release.
  if (!timerButtonArmed) {
    lastTimerButtonState = timerButtonState;
    if (!timerButtonState) {
      timerButtonArmed = true;
      timerButtonPressedAt = 0;
      Serial.println("[TIMER] GPIO25 nach Wartungs-Boot freigegeben");
    }
    return;
  }

  if (timerButtonState == lastTimerButtonState) {
    return;
  }

  lastTimerButtonState = timerButtonState;
  if (timerButtonState) {
    noteRelevantActivity();
    timerButtonPressedAt = now;
    Serial.println("[TIMER] GPIO" + String(TIMER_BUTTON_PIN) + " gedrueckt");
    logWeb("[TIMER] GPIO" + String(TIMER_BUTTON_PIN) + " gedrueckt");
    blinkStatusLed();
    return;
  }

  unsigned long pressedMs = static_cast<unsigned long>(now - timerButtonPressedAt);
  if (pressedMs >= TIMER_BUTTON_LONG_PRESS_MS) {
    clearSleepTimer();
    Serial.println("[TIMER] GPIO" + String(TIMER_BUTTON_PIN) + " langer Druck " + String(pressedMs) + "ms -> Timer geloescht");
    logWeb("[TIMER] GPIO" + String(TIMER_BUTTON_PIN) + " langer Druck " + String(pressedMs) + "ms -> Timer geloescht");
  } else {
    bool timerChanged = addSleepTimerMinutes(TIMER_BUTTON_STEP_MINUTES);
    String resultText = timerChanged
                            ? " -> +" + String(TIMER_BUTTON_STEP_MINUTES) + " min"
                            : " ignoriert";
    Serial.println("[TIMER] GPIO" + String(TIMER_BUTTON_PIN) + " kurzer Druck " + String(pressedMs) + "ms" + resultText);
    logWeb("[TIMER] GPIO" + String(TIMER_BUTTON_PIN) + " kurzer Druck " + String(pressedMs) + "ms" + resultText);
  }
  blinkStatusLed();
}

bool activeCardUnchanged() {
  return activeCardUid.length() > 0;
}

void startActiveCardPlayback() {
  if (activeCardUid == "" || currentFolder == 0 || !audioPlayer.isReady()) {
    return;
  }

  if (!initialDfVolumeApplied || lastVolume < 0 || lastDfVolume < 0) {
    Serial.println("[PLAY] Start wartet: DFPlayer-Initiallautstärke noch nicht angewendet");
    logWeb("[PLAY] Start wartet: DFPlayer-Initiallautstärke noch nicht angewendet");
    return;
  }

  if (!activeCardUnchanged()) {
    Serial.println("[NORMAL] Play ignoriert: keine aktive Karte");
    logWeb("[PLAY] Start ignoriert: keine aktive Karte");
    return;
  }

  uint8_t startTrack = pendingStartTrack > 0 ? pendingStartTrack : 1;
  playStartCount++;
  String startLabel = playStartCount == 1 ? "erster START" : String(playStartCount) + ". START";
  Serial.println("[PLAY] " + startLabel + " -> neuer Play-Befehl");
  logWeb("[PLAY] " + startLabel + " -> neuer Play-Befehl");
  Serial.println("[DFPLAYER] playFolder(" + String(currentFolder) + ", " +
                 String(startTrack) + ")");
  logWeb("[DFPLAYER] playFolder(" + String(currentFolder) + ", " +
         String(startTrack) + ")");
  audioPlayer.playFolderTrack(currentFolder, startTrack,
                              pendingStartHasBookmark ? "BOOKMARK" : "RFID");
  noteRelevantActivity();
  String bookmarkInfo = pendingStartHasBookmark
                            ? " (Bookmark @" + String(pendingStartSeconds) +
                                  "s; Start am Trackanfang)"
                            : "";
  Serial.println("[NORMAL] Spiele Ordner " + String(currentFolder) +
                 " ab Track " + String(startTrack) + bookmarkInfo);
  logWeb("[RFID] Starte Ordner " + String(currentFolder) +
         " Track " + String(startTrack) + bookmarkInfo);

  waitingForPlayButton = false;
  playbackPausedByButton = false;
  logCurrentFolder();

  if (sleepTimerEndsAt > 0) {
    unsigned long remainingSeconds = max(1UL, (sleepTimerEndsAt - millis() + 999) / 1000);
    Serial.println("[NORMAL] Sleep Timer: " + String(remainingSeconds) + "s");
  }
}

void handlePlayButtonPlayback() {
  if (!playButtonState) {
    if (playButtonState != lastPlayButtonState) {
      blinkStatusLed();
      logWeb("[PLAY] GPIO" + String(PLAY_BUTTON_PIN) + " kein GND");
      if (audioPlayer.isPlayingNow()) {
        Serial.println("[PLAY] STOP -> Pause-Befehl");
        logWeb("[PLAY] STOP -> Pause-Befehl");
        writeCurrentBookmark("Play Button Pause");
        audioPlayer.pause();
        playbackPausedByButton = true;
        Serial.println("[NORMAL] GPIO" + String(PLAY_BUTTON_PIN) + " kein GND -> Pause");
        logWeb("[PLAY] GPIO" + String(PLAY_BUTTON_PIN) + " kein GND -> Pause");
      }
    }
    lastPlayButtonState = playButtonState;
    return;
  }

  if (playButtonState != lastPlayButtonState) {
    blinkStatusLed();
    logWeb("[PLAY] GPIO" + String(PLAY_BUTTON_PIN) + " GND erkannt");
  }

  if (waitingForPlayButton) {
    startActiveCardPlayback();
  } else if (playbackPausedByButton && activeCardUnchanged()) {
    playStartCount++;
    String startLabel = playStartCount == 2 ? "zweiter START" : String(playStartCount) + ". START";
    Serial.println("[PLAY] " + startLabel + " -> RESUME");
    logWeb("[PLAY] " + startLabel + " -> RESUME");
    audioPlayer.resume();
    noteRelevantActivity();
    playbackPausedByButton = false;
    Serial.println("[NORMAL] GPIO" + String(PLAY_BUTTON_PIN) + " GND -> Fortsetzen");
    logWeb("[PLAY] GPIO" + String(PLAY_BUTTON_PIN) + " GND -> Fortsetzen");
  }

  lastPlayButtonState = playButtonState;
}

String bookmarkStorageKey(const String& uid) {
  uint32_t hash = 2166136261UL;

  for (int i = 0; i < uid.length(); i++) {
    hash ^= static_cast<uint8_t>(uid.charAt(i));
    hash *= 16777619UL;
  }

  String key = "bm";
  key += String(hash, HEX);
  return key;
}

uint32_t encodeStoredBookmark(const CardBookmark& bookmark) {
  return (static_cast<uint32_t>(bookmark.folder) << 24) |
         (static_cast<uint32_t>(bookmark.track) << 16) |
         bookmark.seconds;
}

CardBookmark decodeStoredBookmark(uint32_t value) {
  CardBookmark bookmark;
  bookmark.folder = (value >> 24) & 0xFF;
  bookmark.track = (value >> 16) & 0xFF;
  bookmark.seconds = value & 0xFFFF;
  bookmark.valid = bookmark.folder != 0 && bookmark.track != 0;
  return bookmark;
}

bool loadLocalBookmark(const String& uid, uint8_t folder, CardBookmark& bookmark) {
  uint32_t value = bookmarkPrefs.getULong(bookmarkStorageKey(uid).c_str(), 0);
  if (value == 0) {
    return false;
  }

  bookmark = decodeStoredBookmark(value);
  if (!bookmark.valid || bookmark.folder != folder) {
    bookmark = CardBookmark{};
    return false;
  }

  return true;
}

void saveLocalBookmark(const String& uid, const CardBookmark& bookmark) {
  if (uid == "" || !bookmark.valid) {
    return;
  }

  bookmarkPrefs.putULong(bookmarkStorageKey(uid).c_str(), encodeStoredBookmark(bookmark));
  Serial.println("[NORMAL] Bookmark lokal gespeichert fuer UID " + uid);
}

void clearLocalBookmark(const String& uid) {
  if (uid == "") {
    return;
  }

  bookmarkPrefs.remove(bookmarkStorageKey(uid).c_str());
  Serial.println("[NORMAL] Bookmark lokal geloescht fuer UID " + uid);
}

bool writeCurrentBookmark(const char* reason) {
  PlaybackPosition position = audioPlayer.getPlaybackPosition();
  if (!position.valid || position.folder == 0 || position.track == 0 || activeCardUid == "") {
    Serial.println("[NORMAL] Bookmark ignoriert: keine Karte/Position");
    return false;
  }

  CardBookmark bookmark;
  bookmark.valid = true;
  bookmark.folder = position.folder;
  bookmark.track = position.track;
  bookmark.seconds = position.seconds;

  saveLocalBookmark(activeCardUid, bookmark);
  rememberDisplayedBookmark(true, bookmark.track, bookmark.seconds);
  Serial.println("[NORMAL] Bookmark gespeichert (" + String(reason) + "): Ordner " +
                 String(bookmark.folder) + " Track " + String(bookmark.track) +
                 " @" + String(bookmark.seconds) + "s");
  return true;
}

bool clearCurrentBookmark(const char* reason) {
  if (activeCardUid == "") {
    Serial.println("[NORMAL] Bookmark loeschen ignoriert: keine aktive Karte");
    return false;
  }

  clearLocalBookmark(activeCardUid);
  rememberDisplayedBookmark(false, 0, 0);
  Serial.println("[NORMAL] Bookmark geloescht (" + String(reason) + ")");
  return true;
}

void updateSleepTimer() {
  if (sleepTimerEndsAt == 0) {
    return;
  }

  long remainingMs = static_cast<long>(sleepTimerEndsAt - millis());

  if (remainingMs <= 0) {
    Serial.println("[POWER] Sleep-Timer abgelaufen");
    bool bookmarkSaved = writeCurrentBookmark("Sleep Timer");
    Serial.println(bookmarkSaved ? "[POWER] Bookmark gespeichert"
                                 : "[POWER] Kein gueltiger Bookmark zu speichern");
    sleepTimerEndsAt = 0;
    lastSleepTimerDrawnSecond = 0;
    currentFolder = 0;
    activeCardUid = "";
    rememberDisplayedBookmark(false, 0, 0);
    powerManager.beginDeepSleep(audioPlayer, rfidManager, webServer, STATUS_LED_PIN);
    return;
  }

  unsigned long remainingSeconds = (static_cast<unsigned long>(remainingMs) + 999) / 1000;

  if (remainingSeconds != lastSleepTimerDrawnSecond) {
    lastSleepTimerDrawnSecond = remainingSeconds;
    if (!notificationActive()) {
      Serial.println("[NORMAL] Sleep Timer: " + String(remainingSeconds) + "s");
    }
  }
}


void handleRFID() {
  const unsigned long now = millis();
  if (static_cast<unsigned long>(now - lastRfidPollAt) < RFID_POLL_MS) {
    return;
  }
  lastRfidPollAt = now;

  if (!rfidManager.update()) {
    if (rfidManager.isCardPresent() && rfidManager.getLastUid().length() > 0) {
      unsigned long now = millis();
      if (rfidManager.getLastUid() != lastDebugCardUid ||
          static_cast<unsigned long>(now - lastDebugCardLoggedAt) >= RFID_DEBUG_REPEAT_MS) {
        lastDebugCardUid = rfidManager.getLastUid();
        lastDebugCardLoggedAt = now;
        logWeb("[RFID] Karte im Feld " + rfidManager.getLastUid() +
               " (" + rfidManager.getLastCardType() + ")");
      }
    } else {
      unsigned long now = millis();
      if (static_cast<unsigned long>(now - lastRfidNoCardLoggedAt) >= RFID_DEBUG_REPEAT_MS) {
        lastRfidNoCardLoggedAt = now;
        String error = rfidManager.getLastError();
        if (error.length() == 0) {
          error = "kein Status";
        }
        logWeb("[RFID] Suche Karte... " + error);
      }
    }

    if (!rfidManager.isCardPresent() && !audioPlayer.isPlayingNow() &&
        !waitingForPlayButton && !playbackPausedByButton) {
      activeCardUid = "";
    }
    return;
  }

  TonuinoCardData card = rfidManager.readTonuinoCard();
  lastDebugCardUid = rfidManager.getLastUid();
  lastDebugCardLoggedAt = millis();
  logWeb("[RFID] Karte erkannt " + rfidManager.getLastUid() +
         " (" + rfidManager.getLastCardType() + ")");
  logRfidDebugDetails();

  if (!card.valid) {
    Serial.println("[NORMAL] Karte nicht lesbar oder Cookie falsch");
    logWeb("[RFID] Karte nicht lesbar oder Cookie falsch");
    return;
  }

  lastTonuinoFolder = card.folder;
  lastTonuinoMode = card.mode;

  if (card.mode != 2) {
    Serial.println("[NORMAL] Nicht unterstuetzter Modus: " + String(card.mode));
    logWeb("[RFID] Nicht unterstuetzter Modus: " + String(card.mode));
    return;
  }

  if (!audioPlayer.isReady()) {
    if (!dfPlayerUnavailableLogged) {
      dfPlayerUnavailableLogged = true;
      Serial.println("[NORMAL] DFPlayer nicht bereit; Initialisierung/Recovery läuft");
      logWeb("[RFID] Karte erkannt, aber DFPlayer-Initialisierung läuft");
    }
    return;
  }
  dfPlayerUnavailableLogged = false;

  if (card.uid == activeCardUid) {
    return;
  }

  noteRelevantActivity();

  if (audioPlayer.isPlayingNow()) {
    writeCurrentBookmark("Kartenwechsel");
    audioPlayer.pause();
  }

  activeCardUid = card.uid;
  currentFolder = card.folder;
  playStartCount = 0;
  waitingForPlayButton = true;
  playbackPausedByButton = false;
  logWeb("[RFID] Karte " + card.uid + " -> Ordner " + String(card.folder) + " " + folderTitle(card.folder));
  pendingStartHasBookmark = false;
  pendingStartTrack = 1;
  pendingStartSeconds = 0;
  rememberDisplayedBookmark(false, 0, 0);
  CardBookmark bookmark;
  if (loadLocalBookmark(card.uid, card.folder, bookmark)) {
    pendingStartHasBookmark = true;
    pendingStartTrack = bookmark.track;
    pendingStartSeconds = bookmark.seconds;
    rememberDisplayedBookmark(true, bookmark.track, bookmark.seconds);
    Serial.println("[NORMAL] Bookmark geladen: Ordner " + String(bookmark.folder) +
                   " Track " + String(bookmark.track) + " @" +
                   String(bookmark.seconds) + "s; Wiedergabe ab Trackanfang");
    logWeb("[BOOKMARK] Track " + String(bookmark.track) + " @" +
           String(bookmark.seconds) + "s geladen");
  }
  Serial.println("[RFID] Kartenordner " + String(card.folder) +
                 " | UID " + card.uid + " | Modus " + String(card.mode));
  Serial.println("[CARD] UID " + card.uid + " Folder " + String(card.folder) +
                 " Mode " + String(card.mode) + " PlayGPIO=" +
                 String(playButtonState ? "LOW/START" : "HIGH/STOP"));
  logWeb("[CARD] UID " + card.uid + " Folder " + String(card.folder) +
         " Mode " + String(card.mode) + " PlayGPIO=" +
         String(playButtonState ? "LOW/START" : "HIGH/STOP"));

  if (playButtonState) {
    startActiveCardPlayback();
  } else {
    Serial.println("[NORMAL] Karte bereit, warte auf GPIO" + String(PLAY_BUTTON_PIN) + " GND");
    logWeb("[RFID] Warte auf GPIO" + String(PLAY_BUTTON_PIN) + " GND");
  }
}

void handleFolderFinished() {
  if (!audioPlayer.consumeFolderFinished()) {
    return;
  }

  clearCurrentBookmark("Ordnerende");
  currentFolder = 0;
  Serial.println("[POWER] Ordner vollständig beendet -> Deep Sleep");
  logWeb("[POWER] Ordner vollständig beendet -> Deep Sleep");
  powerManager.beginDeepSleep(audioPlayer, rfidManager, webServer, STATUS_LED_PIN);
}

void updateInactivitySleep() {
  if (audioPlayer.isPlayingNow()) {
    noteRelevantActivity();
    return;
  }

  if (static_cast<unsigned long>(millis() - lastRelevantActivityAt) <
      INACTIVITY_SLEEP_MS) {
    return;
  }

  Serial.println("[POWER] 10 Minuten keine aktive Wiedergabe -> Deep Sleep");
  logWeb("[POWER] 10 Minuten keine aktive Wiedergabe -> Deep Sleep");
  writeCurrentBookmark("Inaktivität");
  powerManager.beginDeepSleep(audioPlayer, rfidManager, webServer, STATUS_LED_PIN);
}

} // namespace

void NormalMode::begin(bool maintenanceMode, bool bootButtonMustBeReleased) {
  Serial.println("[NORMAL] Starte NormalMode");
  lastRelevantActivityAt = millis();

  rfidManager.begin();
  bookmarkPrefs.begin("bookmarks", false);
  
  if (maintenanceMode) {
    webServer.begin(WIFI_SSID, WIFI_PASS, OTA_NAME, &audioPlayer, &rfidManager);
  } else {
    webServer.disable();
  }
  if (rfidManager.isReaderConnected()) {
    logWeb("[RFID] RC522 Version " + rfidManager.getReaderVersionText() + " erkannt");
    logWeb("[RFID] Warte auf Karte");
  } else {
    logWeb("[RFID] Reader nicht erreichbar, Version " + rfidManager.getReaderVersionText());
    logWeb("[RFID] Pins pruefen: SDA=5 SCK=18 MOSI=23 MISO=19 RST=22 3V3/GND");
  }
  
  analogReadResolution(12);
  analogSetPinAttenuation(VOL_PIN, ADC_11db);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
  statusLedOn = false;
  statusLedOffAt = 0;
  logWeb("[LED] GPIO" + String(STATUS_LED_PIN) + " Status-LED bereit");
  pinMode(PLAY_BUTTON_PIN, INPUT_PULLUP);
  playButtonRawState = digitalRead(PLAY_BUTTON_PIN) == LOW;
  playButtonState = playButtonRawState;
  lastPlayButtonState = playButtonState;
  playButtonRawChangedAt = millis();
  Serial.println("[NORMAL] Play Button GPIO" + String(PLAY_BUTTON_PIN) + " initial " + String(playButtonState ? "TRUE" : "FALSE"));
  logWeb("[PLAY] GPIO" + String(PLAY_BUTTON_PIN) + " initial " + String(playButtonState ? "TRUE" : "FALSE"));
  pinMode(BTN_FROWARD_PIN, INPUT_PULLUP);
  btnForwardRawState = digitalRead(BTN_FROWARD_PIN) == LOW;
  btnForwardState = btnForwardRawState;
  lastBtnForwardState = btnForwardState;
  btnForwardRawChangedAt = millis();
  String btnForwardInitialState = btnForwardState ? "gemeldet" : "frei";
  Serial.println("[BTN_FROWARD] GPIO" + String(BTN_FROWARD_PIN) + " initial " + btnForwardInitialState);
  logWeb("[BTN_FROWARD] GPIO" + String(BTN_FROWARD_PIN) + " initial " + btnForwardInitialState);
  pinMode(BTN_BACK_PIN, INPUT_PULLUP);
  btnBackRawState = digitalRead(BTN_BACK_PIN) == LOW;
  btnBackState = btnBackRawState;
  lastBtnBackState = btnBackState;
  btnBackRawChangedAt = millis();
  String btnBackInitialState = btnBackState ? "gemeldet" : "frei";
  Serial.println("[BTN_BACK] GPIO" + String(BTN_BACK_PIN) + " initial " + btnBackInitialState);
  logWeb("[BTN_BACK] GPIO" + String(BTN_BACK_PIN) + " initial " + btnBackInitialState);
  pinMode(TIMER_BUTTON_PIN, INPUT_PULLUP);
  timerButtonRawState = digitalRead(TIMER_BUTTON_PIN) == LOW;
  timerButtonState = timerButtonRawState;
  lastTimerButtonState = timerButtonState;
  timerButtonRawChangedAt = millis();
  timerButtonPressedAt = timerButtonState ? millis() : 0;
  timerButtonArmed = !(bootButtonMustBeReleased && timerButtonState);
  Serial.println("[TIMER] Timerbutton GPIO" + String(TIMER_BUTTON_PIN) + " initial " + String(timerButtonState ? "gedrueckt" : "frei"));
  logWeb("[TIMER] GPIO" + String(TIMER_BUTTON_PIN) + " initial " + String(timerButtonState ? "gedrueckt" : "frei"));

  audioPlayer.setLogCallback(logWeb);
  initialDfVolumeApplied = false;
  if (audioPlayer.begin()) {
    Serial.println("[NORMAL] " + audioPlayer.getStatusText());
    logWeb("[DFPLAYER] " + audioPlayer.getStatusText());
    int initialRaw = readMedianRaw();
    lastAcceptedVolumeRaw = initialRaw;
    volumeRawCandidate = initialRaw;
    volumeRawCandidateSince = millis();
    filteredVolumeRaw = initialRaw;
    lastVolumeRawLogged = initialRaw;
    lastVolumeRawLoggedAt = millis();
    int initialVolume = rawToVolume(initialRaw);
    int initialDfVolume = rawToDfVolume(initialRaw);
    lastVolume = initialVolume;
    lastDfVolume = initialDfVolume;
    pendingVolume = initialVolume;
    pendingDfVolume = initialDfVolume;
    pendingVolumeChangedAt = millis();
    lastVolumePollAt = millis();
    audioPlayer.setVolume(initialVolume, initialDfVolume, initialRaw);
    delay(DF_VOLUME_COMMAND_SETTLE_MS);
    initialDfVolumeApplied = true;
    String initialPotiLog = "[POTI] RAW=" + String(initialRaw) +
                            " -> logical " + String(initialVolume) + "/10";
    String initialVolumeLog = "[VOLUME] RAW=" + String(initialRaw) +
                              " -> DF " + String(audioPlayer.getMappedDfVolume()) +
                              "/30 (direkt aus RAW, logical " +
                              String(initialVolume) + "/10)";
    Serial.println(initialPotiLog);
    logWeb(initialPotiLog);
    Serial.println(initialVolumeLog);
    logWeb(initialVolumeLog);
  } else {
    Serial.println("[NORMAL] " + audioPlayer.getStatusText());
    logWeb("[DFPLAYER] " + audioPlayer.getStatusText());
  }
}

void NormalMode::update() {
  if (powerManager.isPending()) {
    powerManager.update();
    delay(10);
    return;
  }
  MaintenanceSnapshot maintenance;
  maintenance.activeCardUid = activeCardUid;
  maintenance.currentFolder = currentFolder;
  maintenance.waitingForPlay = waitingForPlayButton;
  maintenance.sleepTimerEndsAt = sleepTimerEndsAt;
  maintenance.playButton = playButtonState;
  maintenance.forwardButton = btnForwardState;
  maintenance.backButton = btnBackState;
  maintenance.timerButton = timerButtonState;
  maintenance.volumeRaw = filteredVolumeRaw;
  maintenance.logicalVolume = lastVolume;
  maintenance.lastTonuinoFolder = lastTonuinoFolder;
  maintenance.lastTonuinoMode = lastTonuinoMode;
  webServer.setSnapshot(maintenance);
  audioPlayer.update();
  if (audioPlayer.consumeRecoveredAfterBoot()) {
    dfPlayerUnavailableLogged = false;
    initialDfVolumeApplied = false;
    int recoveredRaw = filteredVolumeRaw >= 0 ? filteredVolumeRaw : readMedianRaw();
    int recoveredVolume = lastVolume >= 0 ? lastVolume : rawToVolume(recoveredRaw);
    int recoveredDfVolume = lastDfVolume >= 0 ? lastDfVolume : rawToDfVolume(recoveredRaw);
    filteredVolumeRaw = recoveredRaw;
    lastVolume = recoveredVolume;
    lastDfVolume = recoveredDfVolume;
    pendingVolume = recoveredVolume;
    pendingDfVolume = recoveredDfVolume;
    audioPlayer.setVolume(recoveredVolume, recoveredDfVolume, recoveredRaw);
    delay(DF_VOLUME_COMMAND_SETTLE_MS);
    initialDfVolumeApplied = true;
    Serial.println("[NORMAL] DFPlayer-Recovery erfolgreich");
    logWeb("[DFPLAYER] Recovery erfolgreich");
    String recoveryVolumeLog = "[VOLUME] RAW=" + String(recoveredRaw) +
                               " -> DF " + String(audioPlayer.getMappedDfVolume()) +
                               "/30 (Recovery, direkt aus RAW, logical " +
                               String(recoveredVolume) + "/10)";
    Serial.println(recoveryVolumeLog);
    logWeb(recoveryVolumeLog);
  }
  webServer.update();
  handleFolderFinished();
  if (powerManager.isPending()) {
    return;
  }
  updatePlayButton();
  updateBtnForward();
  updateBtnBack();
  updateTimerButton();
  handlePlayButtonPlayback();
  updateStatusLed();
  applyVolume();
  handleRFID();
  updateSleepTimer();
  if (powerManager.isPending()) {
    return;
  }
  updateInactivitySleep();
  updateTemporaryNotification();

  delay(20);
}
