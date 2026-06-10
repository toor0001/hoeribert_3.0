#include "App.h"

#include <Arduino.h>

void App::begin() {
  Serial.begin(115200);
  delay(1000);

  // Starte direkt im Normal Mode
  normalMode.begin();
}

void App::update() {
  normalMode.update();
}
