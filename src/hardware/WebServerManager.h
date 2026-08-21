#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

class AudioPlayer;  // Forward declaration
class RFIDManager;

struct MaintenanceSnapshot {
  String activeCardUid;
  uint8_t currentFolder = 0;
  bool waitingForPlay = false;
  unsigned long sleepTimerEndsAt = 0;
  bool playButton = false;
  bool forwardButton = false;
  bool backButton = false;
  bool timerButton = false;
  int volumeRaw = -1;
  int logicalVolume = -1;
  uint8_t lastTonuinoFolder = 0;
  uint8_t lastTonuinoMode = 0;
};

class WebServerManager {
public:
  void begin(const char* ssid, const char* password, const char* otaName,
             AudioPlayer* audioPlayer, RFIDManager* rfidManager);
  void disable();
  void shutdown();
  void update();
  void setSnapshot(const MaintenanceSnapshot& value);
  void handleRoot();
  void handleStatus();
  void handleLogs();
  void handlePlayerStart();
  void handlePlayerPause();
  void handlePlayerPrevious();
  void handlePlayerNext();
  void log(const String& line);

private:
  static constexpr int LOG_LINE_COUNT = 100;
  static constexpr uint16_t LOG_STREAM_PORT = 2323;
  static constexpr int LOG_STREAM_CLIENT_COUNT = 2;

  WebServer server{80};
  WiFiServer logStreamServer{LOG_STREAM_PORT};
  WiFiClient logStreamClients[LOG_STREAM_CLIENT_COUNT];
  AudioPlayer* player = nullptr;
  RFIDManager* rfid = nullptr;
  bool initialized = false;
  bool servicesStarted = false;
  bool connectTimeoutLogged = false;
  unsigned long wifiStartedAt = 0;
  MaintenanceSnapshot snapshot;
  String logLines[LOG_LINE_COUNT];
  int nextLogLine = 0;
  int storedLogLines = 0;

  String getStatusJSON() const;
  String getLogsJSON() const;
  String jsonEscape(const String& value) const;
  void startNetworkServices();
  void updateLogStream();
  void sendLogStreamLine(const String& line);
  void sendPlayerActionResult(int statusCode, const String& message);
  void logPlayerAction(const String& message);
};
