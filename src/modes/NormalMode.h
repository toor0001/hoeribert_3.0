#pragma once

class NormalMode {
public:
  void begin(bool maintenanceMode, bool bootButtonMustBeReleased);
  void update();
};
