#include "App.h"

#include <Arduino.h>
#include "hardware/HardwarePins.h"

#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <esp_system.h>

namespace {
bool readMaintenanceButtonAtBoot() {
  pinMode(HardwarePins::TIMER_BUTTON, INPUT_PULLUP);
  delay(10);

  uint8_t pressedSamples = 0;
  constexpr uint8_t SAMPLE_COUNT = 5;
  for (uint8_t i = 0; i < SAMPLE_COUNT; i++) {
    if (digitalRead(HardwarePins::TIMER_BUTTON) == LOW) pressedSamples++;
    delay(5);
  }
  return pressedSamples >= 4;
}
}

void App::begin() {
  Serial.begin(115200);
  esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
  bool wokeFromDeepSleepButton = wakeupCause == ESP_SLEEP_WAKEUP_EXT0;
  if (wokeFromDeepSleepButton) {
    rtc_gpio_deinit(GPIO_NUM_25);
  }

  bool buttonPressedAtBoot = readMaintenanceButtonAtBoot();
  maintenanceMode = !wokeFromDeepSleepButton && buttonPressedAtBoot;
  Serial.println();
  if (wokeFromDeepSleepButton) {
    Serial.println("[BOOT] Wakeup aus Deep Sleep via GPIO25");
    Serial.println("[BOOT] Normalbetrieb, WLAN aus");
  } else if (esp_reset_reason() == ESP_RST_POWERON) {
    Serial.println("[BOOT] Power-On");
  } else {
    Serial.println("[BOOT] Reset");
  }
  if (!wokeFromDeepSleepButton) {
    Serial.println(maintenanceMode
                       ? "[BOOT] Wartungsmodus aktiv (GPIO25 beim Boot LOW)"
                       : "[BOOT] Normalbetrieb (GPIO25 beim Boot HIGH, WLAN aus)");
  }

  normalMode.begin(maintenanceMode, buttonPressedAtBoot);
}

void App::update() {
  normalMode.update();
}
