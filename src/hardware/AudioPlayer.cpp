#include "AudioPlayer.h"

void AudioPlayer::setLogCallback(LogCallback callback) {
  logCallback = callback;
}

bool AudioPlayer::begin() {
  dfSerial.begin(9600, SERIAL_8N1, DF_RX_PIN, DF_TX_PIN);
  logInitialization("UART gestartet (9600 Baud, RX GPIO" + String(DF_RX_PIN) +
                    ", TX GPIO" + String(DF_TX_PIN) + ")");
  playing = false;
  folderPlaybackActive = false;
  folderFinished = false;
  currentFolder = 0;
  currentTrack = 0;
  tracksInFolder = 0;
  trackStartedAt = 0;
  trackElapsedBeforePause = 0;
  playbackGeneration = 0;
  lastPlayCommandAt = 0;
  lastPlayFolder = 0;
  lastPlayTrack = 0;
  lastPlaySource = "NONE";
  ready = false;
  recoveredAfterBoot = false;
  initializationAttempts = 0;
  nextInitializationAttemptAt = 0;
  resetFinishHistory();
  statusText = "DFPlayer wartet auf Power-up";

  logInitialization("Power-up-Stabilisierung " + String(POWERUP_STABILIZATION_MS) + " ms");
  delay(POWERUP_STABILIZATION_MS);

  return tryInitialize();
}

void AudioPlayer::update() {
  if (!ready) {
    if (initializationAttempts < MAX_INITIALIZATION_ATTEMPTS &&
        static_cast<int32_t>(millis() - nextInitializationAttemptAt) >= 0) {
      tryInitialize();
    }
    return;
  }

  while (dfPlayer.available()) {
    uint8_t type = dfPlayer.readType();
    uint16_t value = dfPlayer.read();
    unsigned long now = millis();

    Serial.printf("[%lu] DF RX %s value=%u folder=%u track=%u count=%d "
                  "playing=%u generation=%lu source=%s lastPlay=%u/%u playAgeMs=%lu\n",
                  now, eventTypeName(type), value, currentFolder, currentTrack,
                  tracksInFolder, playing,
                  static_cast<unsigned long>(playbackGeneration), lastPlaySource,
                  lastPlayFolder, lastPlayTrack,
                  lastPlayCommandAt == 0 ? 0UL : now - lastPlayCommandAt);

    if (type == DFPlayerPlayFinished) {
      unsigned long finishAgeMs = now - lastAcceptedFinishAt;
      bool duplicateFinish = hasAcceptedFinish &&
                             value == lastAcceptedFinishValue &&
                             finishAgeMs <= DUPLICATE_FINISH_WINDOW_MS;
      if (duplicateFinish) {
        Serial.printf("[%lu] DF FINISH duplicate value=%u ageMs=%lu ignored\n",
                      now, value, finishAgeMs);
        continue;
      }
      hasAcceptedFinish = true;
      lastAcceptedFinishValue = value;
      lastAcceptedFinishAt = now;
      handlePlayFinished();
    } else if (type == DFPlayerError) {
      statusText = "DFPlayer Fehler " + String(value);
      bool endOfUnknownFolder = folderPlaybackActive && tracksInFolder == 0 &&
                                String(lastPlaySource) == "FINISH_EVENT" &&
                                (value == FileIndexOut || value == FileMismatch);
      if (endOfUnknownFolder) {
        playing = false;
        folderPlaybackActive = false;
        folderFinished = true;
        trackStartedAt = 0;
        trackElapsedBeforePause = 0;
        statusText = "Ordner " + String(currentFolder) + " beendet";
        logPlayback("Ordnerende nach nicht vorhandenem Folgetrack erkannt");
        continue;
      }
      if (folderPlaybackActive) {
        playing = false;
        folderPlaybackActive = false;
      }
    }
  }
}

bool AudioPlayer::consumeRecoveredAfterBoot() {
  bool recovered = recoveredAfterBoot;
  recoveredAfterBoot = false;
  return recovered;
}

bool AudioPlayer::tryInitialize() {
  initializationAttempts++;
  logInitialization("Initialisierung Versuch " + String(initializationAttempts) + "/" +
                    String(MAX_INITIALIZATION_ATTEMPTS));

  ready = dfPlayer.begin(dfSerial, true, true);
  if (ready) {
    recoveredAfterBoot = initializationAttempts > 1;
    nextInitializationAttemptAt = 0;
    statusText = "DFPlayer bereit.";
    logInitialization("bereit nach Versuch " + String(initializationAttempts));
    return true;
  }

  if (initializationAttempts < MAX_INITIALIZATION_ATTEMPTS) {
    nextInitializationAttemptAt = millis() + INITIALIZATION_RETRY_MS;
    statusText = "DFPlayer noch nicht erreichbar; Recovery geplant";
    logInitialization("noch nicht erreichbar; neuer Versuch in " +
                      String(INITIALIZATION_RETRY_MS) + " ms");
  } else {
    nextInitializationAttemptAt = 0;
    statusText = "DFPlayer nach " + String(MAX_INITIALIZATION_ATTEMPTS) +
                 " Versuchen nicht erreichbar";
    logInitialization(statusText);
  }
  return false;
}

void AudioPlayer::logInitialization(const String& line) const {
  String message = "[DFPLAYER] " + line;
  Serial.println(message);
  if (logCallback) {
    logCallback(message);
  }
}

bool AudioPlayer::isReady() const {
  return ready;
}

bool AudioPlayer::isPlayingNow() const {
  return playing;
}

void AudioPlayer::playFolder(uint8_t folder, const char* source) {
  if (!ready) return;

  resetFinishHistory();
  folderFinished = false;
  tracksInFolder = dfPlayer.readFileCountsInFolder(folder);
  if (tracksInFolder < 1) {
    tracksInFolder = 0;
  }

  folderPlaybackActive = true;
  startFolderTrack(folder, 1, source);
}


void AudioPlayer::playFolderTrack(uint8_t folder, uint8_t track, const char* source) {
  if (!ready) return;

  resetFinishHistory();
  folderFinished = false;
  if (SKIP_FOLDER_COUNT_QUERY_ON_START) {
    tracksInFolder = 0;
    logPlayback("t=" + String(millis()) +
                "ms Foldercount-Abfrage vor Start übersprungen");
  } else {
    unsigned long queryStartedAt = millis();
    logPlayback("t=" + String(queryStartedAt) + "ms TX readFileCountsInFolder(" +
                String(folder) + ")");
    tracksInFolder = dfPlayer.readFileCountsInFolder(folder);
    logPlayback("t=" + String(millis()) + "ms RX folderCount=" +
                String(tracksInFolder) + " nach " +
                String(millis() - queryStartedAt) + "ms");
    if (tracksInFolder < 1) {
      tracksInFolder = 0;
    }
  }

  folderPlaybackActive = true;
  startFolderTrack(folder, track, source);
}

PlaybackPosition AudioPlayer::getPlaybackPosition() const {
  PlaybackPosition position;

  if (currentFolder == 0 || currentTrack == 0) {
    return position;
  }

  position.valid = true;
  position.folder = currentFolder;
  position.track = currentTrack;
  position.seconds = currentTrackSeconds();
  return position;
}

bool AudioPlayer::consumeFolderFinished() {
  bool finished = folderFinished;
  folderFinished = false;
  return finished;
}

void AudioPlayer::stop() {
  if (!ready) return;

  resetFinishHistory();
  dfPlayer.stop();
  playing = false;
  folderPlaybackActive = false;
  folderFinished = false;
  currentFolder = 0;
  currentTrack = 0;
  trackStartedAt = 0;
  trackElapsedBeforePause = 0;
}

void AudioPlayer::pause() {
  if (!ready) return;

  trackElapsedBeforePause = currentTrackSeconds();
  dfPlayer.pause();
  playing = false;
}

void AudioPlayer::resume() {
  if (!ready) return;
  if (currentFolder == 0 || currentTrack == 0) return;

  dfPlayer.start();
  playing = true;
  trackStartedAt = millis();
}

void AudioPlayer::next() {
  if (!ready) return;

  resetFinishHistory();
  if (folderPlaybackActive && currentFolder > 0 && currentTrack > 0) {
    if (tracksInFolder > 0 && currentTrack >= tracksInFolder) {
      stop();
      return;
    }

    startFolderTrack(currentFolder, currentTrack + 1, "NEXT_BUTTON");
    return;
  }

  dfPlayer.next();
  playing = true;
  trackStartedAt = millis();
  trackElapsedBeforePause = 0;
}

void AudioPlayer::previous() {
  if (!ready) return;

  resetFinishHistory();
  if (folderPlaybackActive && currentFolder > 0 && currentTrack > 1) {
    startFolderTrack(currentFolder, currentTrack - 1, "PREVIOUS_BUTTON");
    return;
  }

  dfPlayer.previous();
  playing = true;
  trackStartedAt = millis();
  trackElapsedBeforePause = 0;
}

void AudioPlayer::setVolume(uint8_t logicalVolume, uint8_t dfVolume) {
  if (!ready) return;

  constexpr uint8_t LOGICAL_MAX_VOLUME = 10;
  constexpr uint8_t DFPLAYER_MAX_VOLUME = 30;
  currentVolume = constrain(logicalVolume, static_cast<uint8_t>(0), LOGICAL_MAX_VOLUME);
  currentDfVolume = constrain(dfVolume, static_cast<uint8_t>(0), DFPLAYER_MAX_VOLUME);
  dfPlayer.volume(currentDfVolume);
}

uint8_t AudioPlayer::getMappedDfVolume() const {
  return currentDfVolume;
}

void AudioPlayer::logPlayback(const String& line) const {
  String message = "[DFPLAYER] " + line;
  Serial.println(message);
  if (logCallback) {
    logCallback(message);
  }
}

AudioPlayerStatus AudioPlayer::readStatus() {
  AudioPlayerStatus status;

  if (!ready) {
    return status;
  }

  status.state = dfPlayer.readState();
  status.volume = currentVolume;
  status.currentFile = dfPlayer.readCurrentFileNumber();
  status.fileCount = dfPlayer.readFileCounts();

  return status;
}

AudioPlayerStatus AudioPlayer::getCachedStatus() const {
  AudioPlayerStatus status;
  status.state = playing ? 1 : (currentFolder > 0 ? 2 : 0);
  status.volume = currentVolume;
  status.currentFile = currentTrack;
  status.fileCount = tracksInFolder;
  status.folder = currentFolder;
  status.track = currentTrack;
  status.tracksInFolder = tracksInFolder;
  return status;
}

void AudioPlayer::prepareForDeepSleep() {
  if (!ready) return;
  stop();
  delay(50);
  dfPlayer.sleep();
  delay(100);
}

String AudioPlayer::getStatusText() const {
  return statusText;
}

void AudioPlayer::startFolderTrack(uint8_t folder, uint8_t track, const char* source) {
  unsigned long now = millis();
  uint8_t previousFolder = currentFolder;
  uint8_t previousTrack = currentTrack;
  playbackGeneration++;
  lastPlayCommandAt = now;
  lastPlayFolder = folder;
  lastPlayTrack = track;
  lastPlaySource = source;
  Serial.printf("[%lu] DF TX PLAY source=%s folder=%u track=%u previous=%u/%u generation=%lu\n",
                now, source, folder, track, previousFolder, previousTrack,
                static_cast<unsigned long>(playbackGeneration));
  logPlayback("t=" + String(now) + "ms TX playFolder(" + String(folder) +
              ", " + String(track) + ") source=" + source);
  dfPlayer.playFolder(folder, track);
  currentFolder = folder;
  currentTrack = track;
  playing = true;
  folderFinished = false;
  trackStartedAt = millis();
  trackElapsedBeforePause = 0;
  statusText = "Ordner " + String(folder) + " Track " + String(track);
}

uint16_t AudioPlayer::currentTrackSeconds() const {
  uint32_t seconds = trackElapsedBeforePause;

  if (playing && trackStartedAt > 0) {
    seconds += (millis() - trackStartedAt) / 1000UL;
  }

  return seconds > 65535UL ? 65535 : static_cast<uint16_t>(seconds);
}

void AudioPlayer::handlePlayFinished() {
  if (!folderPlaybackActive || currentFolder == 0 || currentTrack == 0) {
    playing = false;
    return;
  }

  if (tracksInFolder == 0 && currentTrack < 255) {
    startFolderTrack(currentFolder, currentTrack + 1, "FINISH_EVENT");
    return;
  }

  if (tracksInFolder > 0 && currentTrack < tracksInFolder) {
    startFolderTrack(currentFolder, currentTrack + 1, "FINISH_EVENT");
    return;
  }

  playing = false;
  folderPlaybackActive = false;
  folderFinished = true;
  trackStartedAt = 0;
  trackElapsedBeforePause = 0;
  statusText = "Ordner " + String(currentFolder) + " beendet";
}

const char* AudioPlayer::eventTypeName(uint8_t type) const {
  switch (type) {
    case TimeOut: return "TIMEOUT";
    case WrongStack: return "WRONG_STACK";
    case DFPlayerCardInserted: return "CARD_INSERTED";
    case DFPlayerCardRemoved: return "CARD_REMOVED";
    case DFPlayerCardOnline: return "CARD_ONLINE";
    case DFPlayerUSBInserted: return "USB_INSERTED";
    case DFPlayerUSBRemoved: return "USB_REMOVED";
    case DFPlayerPlayFinished: return "FINISH";
    case DFPlayerError: return "ERROR";
    default: return "OTHER";
  }
}

void AudioPlayer::resetFinishHistory() {
  hasAcceptedFinish = false;
  lastAcceptedFinishValue = 0;
  lastAcceptedFinishAt = 0;
}
