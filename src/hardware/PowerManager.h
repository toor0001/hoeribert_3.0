#pragma once

#include <Arduino.h>

class AudioPlayer;
class RFIDManager;
class WebServerManager;

class PowerManager {
public:
  void beginDeepSleep(AudioPlayer& audio, RFIDManager& rfid,
                      WebServerManager& network, uint8_t statusLedPin);
  void update();
  bool isPending() const;

private:
  bool pending = false;
  unsigned long buttonReleasedAt = 0;
};
