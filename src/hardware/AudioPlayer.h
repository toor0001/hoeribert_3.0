#pragma once

#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>
#include <HardwareSerial.h>
#include "HardwarePins.h"

struct AudioPlayerStatus {
  int state = 0;
  int volume = 0;
  int currentFile = 0;
  int fileCount = 0;
  uint8_t folder = 0;
  uint8_t track = 0;
  int tracksInFolder = 0;
};

struct PlaybackPosition {
  bool valid = false;
  uint8_t folder = 0;
  uint8_t track = 0;
  uint16_t seconds = 0;
};

class AudioPlayer {
public:
  using LogCallback = void (*)(const String& line);

  void setLogCallback(LogCallback callback);
  bool begin();
  void update();
  bool consumeRecoveredAfterBoot();
  bool isReady() const;
  bool isPlayingNow() const;
  void playFolder(uint8_t folder, const char* source = "OTHER");
  void playFolderTrack(uint8_t folder, uint8_t track, const char* source = "OTHER");
  PlaybackPosition getPlaybackPosition() const;
  bool consumeFolderFinished();
  void stop();
  void pause();
  void resume();
  void next();
  void previous();
  void setVolume(uint8_t logicalVolume, uint8_t dfVolume, int rawVolume);
  uint8_t getMappedDfVolume() const;
  AudioPlayerStatus readStatus();
  AudioPlayerStatus getCachedStatus() const;
  void prepareForDeepSleep();
  String getStatusText() const;

private:
  static constexpr uint8_t DF_RX_PIN = HardwarePins::DFPLAYER_RX;
  static constexpr uint8_t DF_TX_PIN = HardwarePins::DFPLAYER_TX;
  static constexpr unsigned long DUPLICATE_FINISH_WINDOW_MS = 500;
  static constexpr unsigned long POWERUP_STABILIZATION_MS = 1500;
  static constexpr unsigned long INITIALIZATION_RETRY_MS = 750;
  static constexpr unsigned long VOLUME_COMMAND_SETTLE_MS = 120;
  static constexpr uint8_t MAX_INITIALIZATION_ATTEMPTS = 3;
  // The real module reports an unreliable folder count. Keep the synchronous
  // query out of the latency-sensitive playback start path.
  static constexpr bool SKIP_FOLDER_COUNT_QUERY_ON_START = true;

  HardwareSerial dfSerial{2};
  DFRobotDFPlayerMini dfPlayer;
  bool ready = false;
  bool recoveredAfterBoot = false;
  uint8_t initializationAttempts = 0;
  unsigned long nextInitializationAttemptAt = 0;
  LogCallback logCallback = nullptr;
  bool playing = false;
  bool folderPlaybackActive = false;
  bool folderFinished = false;
  uint8_t currentFolder = 0;
  uint8_t currentTrack = 0;
  int tracksInFolder = 0;
  uint8_t currentVolume = 0;
  uint8_t currentDfVolume = 0;
  int currentVolumeRaw = -1;
  unsigned long trackStartedAt = 0;
  uint16_t trackElapsedBeforePause = 0;
  String statusText = "DFPlayer nicht gestartet";
  uint32_t playbackGeneration = 0;
  unsigned long lastPlayCommandAt = 0;
  uint8_t lastPlayFolder = 0;
  uint8_t lastPlayTrack = 0;
  const char* lastPlaySource = "NONE";
  bool hasAcceptedFinish = false;
  uint16_t lastAcceptedFinishValue = 0;
  unsigned long lastAcceptedFinishAt = 0;

  void startFolderTrack(uint8_t folder, uint8_t track, const char* source);
  void prepareVolumeForFolderStart();
  bool tryInitialize();
  void logInitialization(const String& line) const;
  void logPlayback(const String& line) const;
  uint16_t currentTrackSeconds() const;
  void handlePlayFinished();
  const char* eventTypeName(uint8_t type) const;
  void resetFinishHistory();
};
