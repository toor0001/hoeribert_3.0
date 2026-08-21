#include "PowerManager.h"

#include "AudioPlayer.h"
#include "HardwarePins.h"
#include "RFIDManager.h"
#include "WebServerManager.h"

#include <driver/rtc_io.h>
#include <esp_sleep.h>

void PowerManager::beginDeepSleep(AudioPlayer& audio, RFIDManager& rfid,
                                  WebServerManager& network, uint8_t statusLedPin) {
  if (pending) return;
  pending = true;
  buttonReleasedAt = 0;

  digitalWrite(statusLedPin, LOW);
  Serial.println("[POWER] Status-LED aus");
  audio.prepareForDeepSleep();
  Serial.println("[POWER] DFPlayer gestoppt und Sleep-Kommando gesendet");
  rfid.powerDown();
  Serial.println("[POWER] RC522 Antenne aus / SoftPowerDown");
  network.shutdown();
  Serial.println("[POWER] WLAN, HTTP, OTA und TCP-Log aus");
  Serial.println("[POWER] Warte auf Freigabe von GPIO25");
}

void PowerManager::update() {
  if (!pending) return;

  unsigned long now = millis();
  if (digitalRead(HardwarePins::TIMER_BUTTON) == LOW) {
    buttonReleasedAt = 0;
    return;
  }
  if (buttonReleasedAt == 0) {
    buttonReleasedAt = now;
    return;
  }
  if (now - buttonReleasedAt < 50) return;

  constexpr gpio_num_t wakePin = GPIO_NUM_25;
  rtc_gpio_init(wakePin);
  rtc_gpio_set_direction(wakePin, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(wakePin);
  rtc_gpio_pulldown_dis(wakePin);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

  esp_err_t result = esp_sleep_enable_ext0_wakeup(wakePin, 0);
  if (result != ESP_OK) {
    Serial.println("[POWER] FEHLER: EXT0-Wakeup konnte nicht aktiviert werden");
    Serial.println("[POWER] Neustart, da Peripherie bereits heruntergefahren ist");
    Serial.flush();
    delay(20);
    ESP.restart();
  }

  Serial.println("[POWER] Deep Sleep -> Wakeup GPIO25 LOW");
  Serial.flush();
  delay(20);
  esp_deep_sleep_start();
}

bool PowerManager::isPending() const {
  return pending;
}
