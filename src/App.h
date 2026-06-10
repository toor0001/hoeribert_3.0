#pragma once

#include <Arduino.h>

#include "modes/NormalMode.h"

class App {
public:
  void begin();
  void update();

private:
  NormalMode normalMode;
};
