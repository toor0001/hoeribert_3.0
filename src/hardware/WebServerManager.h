#pragma once

#include <Arduino.h>
#include <WebServer.h>

class AudioPlayer;  // Forward declaration

class WebServerManager {
public:
  void begin(const char* ssid, const char* password, AudioPlayer* audioPlayer);
  void update();
  void handleRoot();
  void handleStatus();
  void handleControl();
  void handleLogs();
  void handlePanelImage();

private:
  static constexpr int LOG_LINE_COUNT = 18;

  WebServer server{80};
  AudioPlayer* player = nullptr;
  bool initialized = false;
  String logLines[LOG_LINE_COUNT];
  int nextLogLine = 0;
  int storedLogLines = 0;

  String getStatusJSON() const;
  String getStatusText(int state) const;
  String getLogsJSON() const;
  String jsonEscape(const String& value) const;
  void appendLog(const String& line);
};
