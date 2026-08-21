#include <Arduino.h>
#include <ArduinoOTA.h>
#include <MFRC522.h>
#include <SPI.h>
#include <WiFi.h>

#include "secrets.h"
#include "hardware/HardwarePins.h"

namespace {

constexpr uint8_t VOL_PIN = HardwarePins::VOLUME;
constexpr uint8_t PLAY_BUTTON_PIN = HardwarePins::PLAY_BUTTON;
constexpr uint8_t BTN_FORWARD_PIN = HardwarePins::FORWARD_BUTTON;
constexpr uint8_t BTN_BACK_PIN = HardwarePins::BACK_BUTTON;
constexpr uint8_t TIMER_BUTTON_PIN = HardwarePins::TIMER_BUTTON;
constexpr uint8_t STATUS_LED_PIN = HardwarePins::STATUS_LED;
constexpr uint8_t GPIO35_TEST_PIN = 35;

constexpr uint8_t RFID_SS_PIN = HardwarePins::RFID_SS;
constexpr uint8_t RFID_RST_PIN = HardwarePins::RFID_RST;
constexpr uint8_t RFID_SCK_PIN = HardwarePins::RFID_SCK;
constexpr uint8_t RFID_MISO_PIN = HardwarePins::RFID_MISO;
constexpr uint8_t RFID_MOSI_PIN = HardwarePins::RFID_MOSI;

constexpr unsigned long DIGITAL_POLL_MS = 10;
constexpr unsigned long RFID_POLL_MS = 250;
constexpr unsigned long RFID_REPEAT_MS = 2000;
constexpr unsigned long VOLUME_POLL_MS = 80;
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 12000;
constexpr uint16_t LOG_STREAM_PORT = 2323;
constexpr int VOLUME_LOG_DELTA = 45;
constexpr int VOLUME_CANDIDATE_DELTA = 30;
constexpr int VOLUME_RAW_MIN = 40;
constexpr int VOLUME_RAW_MAX = 4050;
constexpr int VOLUME_SAMPLE_COUNT = 9;
constexpr unsigned long VOLUME_STABLE_MS = 260;

struct DigitalInput {
  uint8_t pin;
  const char* function;
  bool pullup;
  bool activeLow;
  bool rawState;
  bool stableState;
  unsigned long changedAt;
};

DigitalInput inputs[] = {
  {PLAY_BUTTON_PIN, "Play/Pause Freigabe", true, true, false, false, 0},
  {BTN_FORWARD_PIN, "Naechster Titel", true, true, false, false, 0},
  {BTN_BACK_PIN, "Vorheriger Titel", true, true, false, false, 0},
  {TIMER_BUTTON_PIN, "Sleeptimer", true, true, false, false, 0},
  {GPIO35_TEST_PIN, "GPIO35 Test/Frei (nur Eingang, externer Pullup/Pulldown noetig)", false, true, false, false, 0},
};

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
MFRC522::MIFARE_Key rfidKey;
WiFiServer logServer(LOG_STREAM_PORT);
WiFiClient logClient;

unsigned long lastDigitalPollAt = 0;
unsigned long lastRfidPollAt = 0;
unsigned long lastRfidLogAt = 0;
unsigned long lastVolumePollAt = 0;
String lastRfidUid = "";
byte rfidReaderVersion = 0;
int lastVolumeRaw = -1;
int lastVolumePercent = -1;
int volumeCandidateRaw = -1;
int volumeCandidatePercent = -1;
int lastPlayRawStatus = -1;
unsigned long volumeCandidateSince = 0;
bool ledOn = false;
bool otaReady = false;

String hexByte(uint8_t value) {
  String text = String(value, HEX);
  text.toUpperCase();
  if (text.length() < 2) {
    text = "0" + text;
  }
  return text;
}

String bytesToHexLine(const byte* data, byte length) {
  String text = "";
  for (byte i = 0; i < length; i++) {
    if (i > 0) {
      text += " ";
    }
    text += hexByte(data[i]);
  }
  return text;
}

String uidToString(MFRC522::Uid* uid) {
  return bytesToHexLine(uid->uidByte, uid->size);
}

String volumeStatusLine(int raw) {
  const int percent = constrain(map(raw, VOLUME_RAW_MIN, VOLUME_RAW_MAX, 0, 100), 0, 100);
  return "[GPIO] GPIO" + String(VOL_PIN) +
         " erkannt -> Lautstaerkeregler | RAW=" + String(raw) +
         " | ca. " + String(percent) + "%";
}

String rfidStatusLine() {
  return "[RFID] RC522 Version 0x" + hexByte(rfidReaderVersion) +
         String((rfidReaderVersion != 0x00 && rfidReaderVersion != 0xFF)
                    ? " erkannt"
                    : " NICHT erreichbar") +
         " | Pins SDA/SS=GPIO5 RST=GPIO22 SCK=GPIO18 MISO=GPIO19 MOSI=GPIO23";
}

String inputStatusLine() {
  String text = "[STATUS] Aktuell";
  for (const auto& input : inputs) {
    text += " | GPIO";
    text += String(input.pin);
    text += "=";
    text += digitalRead(input.pin) == LOW ? "LOW/GND" : "HIGH/offen";
    text += " (";
    text += input.function;
    text += ")";
  }
  text += " | GPIO";
  text += String(VOL_PIN);
  text += "=ADC RAW=";
  text += String(analogRead(VOL_PIN));
  text += " (Lautstaerkeregler)";
  return text;
}

void updateLogClient() {
  if (logClient && logClient.connected()) {
    return;
  }

  if (logClient) {
    logClient.stop();
  }

  WiFiClient newClient = logServer.available();
  if (newClient) {
    logClient = newClient;
    logClient.println("[NC] GPIO/Funktions-Diagnose verbunden");
    logClient.println(inputStatusLine());
    logClient.println(volumeStatusLine(analogRead(VOL_PIN)));
    logClient.println(rfidStatusLine());
  }
}

void logLine(const String& text) {
  Serial.println(text);
  updateLogClient();
  if (logClient && logClient.connected()) {
    logClient.println(text);
  }
}

void logPinEvent(const DigitalInput& input, bool state) {
  const bool active = input.activeLow ? state == LOW : state == HIGH;
  logLine("[GPIO] GPIO" + String(input.pin) + " erkannt -> " +
          String(input.function) +
          " | Zustand=" + String(state == LOW ? "LOW/GND" : "HIGH/offen") +
          " | Aktion=" + String(active ? "AKTIV" : "inaktiv"));
}

void setupDigitalInputs() {
  for (auto& input : inputs) {
    pinMode(input.pin, input.pullup ? INPUT_PULLUP : INPUT);
    input.rawState = digitalRead(input.pin);
    input.stableState = input.rawState;
    input.changedAt = millis();

    logLine("[SETUP] GPIO" + String(input.pin) + " -> " +
            String(input.function) +
            " | Modus=" + String(input.pullup ? "INPUT_PULLUP" : "INPUT"));
  }
}

void updateDigitalInputs() {
  const unsigned long now = millis();
  if (now - lastDigitalPollAt < DIGITAL_POLL_MS) {
    return;
  }
  lastDigitalPollAt = now;

  for (auto& input : inputs) {
    const bool raw = digitalRead(input.pin);
    if (raw != input.rawState) {
      input.rawState = raw;
      input.changedAt = now;
    }

    if (raw != input.stableState && now - input.changedAt >= 35) {
      input.stableState = raw;
      logPinEvent(input, input.stableState);
    }
  }
}

void logPlayRawStatusOnChange() {
  const int raw = digitalRead(PLAY_BUTTON_PIN);
  if (raw == lastPlayRawStatus) {
    return;
  }

  lastPlayRawStatus = raw;
  logLine("[STATUS] GPIO26 roh=" + String(raw == LOW ? "LOW/GND" : "HIGH/offen") +
          " -> Play/Pause Freigabe");
}

int readVolumeMedianRaw() {
  int samples[VOLUME_SAMPLE_COUNT] = {};
  for (int i = 0; i < VOLUME_SAMPLE_COUNT; i++) {
    samples[i] = analogRead(VOL_PIN);
    delayMicroseconds(700);
  }

  for (int i = 1; i < VOLUME_SAMPLE_COUNT; i++) {
    const int value = samples[i];
    int j = i - 1;
    while (j >= 0 && samples[j] > value) {
      samples[j + 1] = samples[j];
      j--;
    }
    samples[j + 1] = value;
  }

  return samples[VOLUME_SAMPLE_COUNT / 2];
}

void updateVolume() {
  const unsigned long now = millis();
  if (now - lastVolumePollAt < VOLUME_POLL_MS) {
    return;
  }
  lastVolumePollAt = now;

  const int raw = readVolumeMedianRaw();
  const int percent = constrain(map(raw, VOLUME_RAW_MIN, VOLUME_RAW_MAX, 0, 100), 0, 100);

  if (lastVolumeRaw >= 0 &&
      abs(raw - lastVolumeRaw) < VOLUME_LOG_DELTA &&
      abs(percent - lastVolumePercent) < 2) {
    volumeCandidateRaw = -1;
    return;
  }

  if (volumeCandidateRaw < 0 ||
      abs(raw - volumeCandidateRaw) >= VOLUME_CANDIDATE_DELTA ||
      abs(percent - volumeCandidatePercent) >= 2) {
    volumeCandidateRaw = raw;
    volumeCandidatePercent = percent;
    volumeCandidateSince = now;
    return;
  }

  if (lastVolumeRaw < 0 || now - volumeCandidateSince >= VOLUME_STABLE_MS) {
    lastVolumeRaw = raw;
    lastVolumePercent = percent;
    volumeCandidateRaw = -1;

    logLine(volumeStatusLine(raw));
  }
}

void setupRfid() {
  SPI.begin(RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN, RFID_SS_PIN);
  rfid.PCD_Init();
  delay(50);
  rfid.PCD_AntennaOn();
  rfid.PCD_SetAntennaGain(rfid.RxGain_max);

  for (byte i = 0; i < 6; i++) {
    rfidKey.keyByte[i] = 0xFF;
  }

  rfidReaderVersion = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  logLine("[SETUP] GPIO5 -> RFID SDA/SS");
  logLine("[SETUP] GPIO22 -> RFID RST");
  logLine("[SETUP] GPIO18 -> RFID SCK");
  logLine("[SETUP] GPIO19 -> RFID MISO");
  logLine("[SETUP] GPIO23 -> RFID MOSI");
  logLine(rfidStatusLine());
}

void updateRfid() {
  const unsigned long now = millis();
  if (now - lastRfidPollAt < RFID_POLL_MS) {
    return;
  }
  lastRfidPollAt = now;

  rfid.PCD_StopCrypto1();
  bool selected = rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial();
  if (!selected) {
    byte atqa[2] = {};
    byte atqaSize = sizeof(atqa);
    const MFRC522::StatusCode status = rfid.PICC_WakeupA(atqa, &atqaSize);
    selected = status == MFRC522::STATUS_OK && rfid.PICC_ReadCardSerial();
  }

  if (!selected) {
    return;
  }

  const String uid = uidToString(&rfid.uid);
  if (uid == lastRfidUid && now - lastRfidLogAt < RFID_REPEAT_MS) {
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;
  }

  lastRfidUid = uid;
  lastRfidLogAt = now;

  const MFRC522::PICC_Type type = rfid.PICC_GetType(rfid.uid.sak);
  logLine("[RFID] Karte erkannt -> GPIO5/18/19/23/22 | UID=" + uid +
          " | Typ=" + String(rfid.PICC_GetTypeName(type)) +
          " | SAK=0x" + hexByte(rfid.uid.sak));

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

void updateStatusLed() {
  static unsigned long lastBlinkAt = 0;
  const unsigned long now = millis();
  if (now - lastBlinkAt < 500) {
    return;
  }
  lastBlinkAt = now;
  ledOn = !ledOn;
  digitalWrite(STATUS_LED_PIN, ledOn ? HIGH : LOW);
}

void setupNetwork() {
  // Wichtig fuer dieses Diagnoseprogramm: OTA und nc-Logserver bleiben dauerhaft aktiv.
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startedAt < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Nicht verbunden, nc/OTA nicht aktiv");
    return;
  }

  logServer.begin();
  logServer.setNoDelay(true);
  Serial.print("[WIFI] Verbunden IP=");
  Serial.println(WiFi.localIP());
  Serial.print("[NC] Logserver: nc ");
  Serial.print(WiFi.localIP());
  Serial.print(" ");
  Serial.println(LOG_STREAM_PORT);

  ArduinoOTA.setHostname(OTA_NAME);
  ArduinoOTA.begin();
  otaReady = true;
  Serial.println("[OTA] Bereit");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("[TEST] GPIO/Funktions-Diagnose gestartet");
  Serial.println("[TEST] Ausgabeformat: GPIO erkannt -> zugewiesene Funktion");
  setupNetwork();

  analogReadResolution(12);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
  logLine("[SETUP] GPIO" + String(STATUS_LED_PIN) + " -> Status-LED");

  setupDigitalInputs();
  setupRfid();
  logLine(volumeStatusLine(analogRead(VOL_PIN)));
  logLine("[TEST] Bereit. Buttons druecken, Poti bewegen oder RFID-Karte auflegen.");
}

void loop() {
  updateLogClient();
  if (otaReady) {
    ArduinoOTA.handle();
  }
  updateDigitalInputs();
  logPlayRawStatusOnChange();
  updateVolume();
  updateRfid();
  updateStatusLed();
}
