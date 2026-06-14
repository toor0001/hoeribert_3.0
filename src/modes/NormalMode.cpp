#include "NormalMode.h"

#include "hardware/AudioPlayer.h"
#include "hardware/RFIDManager.h"
#include "hardware/WebServerManager.h"
#include "secrets.h"

#include <Arduino.h>
#include <Preferences.h>
#include <math.h>

namespace {

constexpr uint8_t VOL_PIN = 34;
constexpr int VOL_RAW_MIN = 40;
constexpr int VOL_RAW_MAX = 4050;
constexpr int VOL_MAX = 24;

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
Preferences bookmarkPrefs;

uint8_t currentFolder = 0;
int lastVolume = -1;
int filteredVolumeRaw = -1;
String activeCardUid = "";
bool activeBookmarkValid = false;
uint8_t activeBookmarkTrack = 0;
uint16_t activeBookmarkSeconds = 0;
unsigned long sleepTimerEndsAt = 0;
unsigned long lastSleepTimerDrawnSecond = 0;
unsigned long notificationEndsAt = 0;
constexpr unsigned long NOTIFICATION_MS = 3000;

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
    Serial.println("[NORMAL] Volume " + String(volume) + "/" + String(VOL_MAX) +
                   " RAW=" + String(raw) + " FILT=" + String(filteredVolumeRaw));
  }
}

void logCurrentFolder() {
  Serial.println("[NORMAL] Ordner: " + String(currentFolder) + " - " + folderTitle(currentFolder));
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

void addSleepTimerMinutes(uint8_t minutes) {
  if (!audioPlayer.isPlayingNow()) {
    Serial.println("[NORMAL] Sleep Timer ignoriert: keine Wiedergabe");
    return;
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
}

void clearSleepTimer() {
  sleepTimerEndsAt = 0;
  lastSleepTimerDrawnSecond = 0;
  Serial.println("[NORMAL] Sleep Timer geloescht");
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
    writeCurrentBookmark("Sleep Timer");
    sleepTimerEndsAt = 0;
    lastSleepTimerDrawnSecond = 0;
    audioPlayer.stop();
    currentFolder = 0;
    activeCardUid = "";
    rememberDisplayedBookmark(false, 0, 0);
    Serial.println("[NORMAL] Sleep Timer abgelaufen -> Stop");
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
  if (!rfidManager.update()) {
    if (!rfidManager.isCardPresent() && !audioPlayer.isPlayingNow()) {
      activeCardUid = "";
    }
    return;
  }

  TonuinoCardData card = rfidManager.readTonuinoCard();

  if (!card.valid) {
    Serial.println("[NORMAL] Karte nicht lesbar oder Cookie falsch");
    return;
  }

  if (card.mode != 2) {
    Serial.println("[NORMAL] Nicht unterstuetzter Modus: " + String(card.mode));
    return;
  }

  if (!audioPlayer.isReady()) {
    Serial.println("[NORMAL] DFPlayer nicht bereit");
    return;
  }

  if (card.uid == activeCardUid) {
    return;
  }

  activeCardUid = card.uid;
  currentFolder = card.folder;
  CardBookmark localBookmark;
  bool hasLocalBookmark = loadLocalBookmark(card.uid, card.folder, localBookmark);
  bool hasBookmark = hasLocalBookmark;
  uint8_t startTrack = localBookmark.track;
  uint16_t startSeconds = localBookmark.seconds;
  rememberDisplayedBookmark(hasBookmark, startTrack, startSeconds);

  if (hasBookmark) {
    audioPlayer.playFolderTrack(currentFolder, startTrack);
    Serial.println("[NORMAL] Spiele Ordner " + String(currentFolder) +
                   " ab Bookmark Track " + String(startTrack) +
                   " @" + String(startSeconds) + "s");
  } else {
    audioPlayer.playFolder(currentFolder);
    Serial.println("[NORMAL] Spiele Ordner " + String(currentFolder));
  }

  logCurrentFolder();

  if (sleepTimerEndsAt > 0) {
    unsigned long remainingSeconds = max(1UL, (sleepTimerEndsAt - millis() + 999) / 1000);
    Serial.println("[NORMAL] Sleep Timer: " + String(remainingSeconds) + "s");
  }
}

void handleFolderFinished() {
  if (!audioPlayer.consumeFolderFinished()) {
    return;
  }

  clearCurrentBookmark("Ordnerende");
  currentFolder = 0;
  Serial.println("[NORMAL] Ordner beendet");
}

} // namespace

void NormalMode::begin() {
  Serial.println("[NORMAL] Starte NormalMode");

  rfidManager.begin();
  bookmarkPrefs.begin("bookmarks", false);
  
  // Starte Web-Interface statt WiFi zu deaktivieren
  webServer.begin(WIFI_SSID, WIFI_PASS, &audioPlayer);
  
  analogReadResolution(12);
  analogSetPinAttenuation(VOL_PIN, ADC_11db);

  if (audioPlayer.begin()) {
    Serial.println("[NORMAL] " + audioPlayer.getStatusText());
    int initialVolume = rawToVolume(readAverageRaw());
    lastVolume = initialVolume;
    audioPlayer.setVolume(initialVolume);
    Serial.println("[NORMAL] Startvolume " + String(initialVolume) + "/" + String(VOL_MAX));
  } else {
    Serial.println("[NORMAL] " + audioPlayer.getStatusText());
  }
}

void NormalMode::update() {
  audioPlayer.update();
  webServer.update();
  handleFolderFinished();
  applyVolume();
  handleRFID();
  updateSleepTimer();
  updateTemporaryNotification();

  delay(20);
}
