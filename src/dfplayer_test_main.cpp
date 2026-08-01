#include <Arduino.h>
#include <ArduinoOTA.h>
#include <HardwareSerial.h>
#include <MFRC522.h>
#include <SPI.h>
#include <WiFi.h>
#include <math.h>

#include "secrets.h"

namespace {

constexpr uint8_t DF_RX_PIN = 16;
constexpr uint8_t DF_TX_PIN = 17;
constexpr uint8_t VOL_PIN = 34;
constexpr uint8_t GPIO32_TEST_PIN = 32;
constexpr uint8_t RFID_SS_PIN = 5;
constexpr uint8_t RFID_RST_PIN = 22;
constexpr uint8_t RFID_SCK_PIN = 18;
constexpr uint8_t RFID_MISO_PIN = 19;
constexpr uint8_t RFID_MOSI_PIN = 23;
constexpr unsigned long RFID_POLL_MS = 250;
constexpr unsigned long RFID_REPEAT_LOG_MS = 3000;
constexpr int VOL_RAW_MIN = 2;
constexpr int VOL_RAW_MAX = 4050;
constexpr int VOL_MAX = 10;
constexpr int VOL_MIN_AUDIBLE = 1;
constexpr int DF_VOLUME_MAX = 24;
constexpr float VOL_RESPONSE_CURVE = 0.28f;
constexpr unsigned long VOL_POLL_MS = 120;
constexpr unsigned long VOL_STEP_STABLE_MS = 120;
constexpr int VOL_RAW_LOG_DELTA = 12;
constexpr int VOL_RAIL_LOW = 1;
constexpr int VOL_RAIL_HIGH = 4070;
constexpr int VOL_GLITCH_JUMP = 900;
constexpr unsigned long VOL_RAIL_STABLE_MS = 350;
constexpr unsigned long GPIO32_DEBOUNCE_MS = 40;
constexpr uint8_t STATUS_LED_PIN = 25;
constexpr uint8_t BTN_FROWARD_PIN = 13;
constexpr uint8_t BTN_BACK_PIN = 15;
constexpr unsigned long TRACK_BUTTON_DEBOUNCE_MS = 40;
constexpr uint8_t TIMER_BUTTON_PIN = 33;
constexpr unsigned long TIMER_BUTTON_DEBOUNCE_MS = 50;
constexpr unsigned long TIMER_BUTTON_LONG_PRESS_MS = 1200;
constexpr uint8_t TIMER_BUTTON_STEP_MINUTES = 10;
constexpr unsigned long LED_BLINK_MS = 500;
constexpr unsigned long DF_POWERUP_DELAY_MS = 3000;
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 12000;
constexpr unsigned long PLAY_START_DELAY_MS = 3000;
constexpr unsigned long PLAY_RETRY_MS = 1500;
constexpr uint16_t LOG_STREAM_PORT = 2323;
constexpr uint8_t PLAY_ATTEMPTS = 1;
constexpr bool DF_REQUEST_FEEDBACK = true;
constexpr unsigned long DF_FEEDBACK_FRAME_TIMEOUT_MS = 120;

HardwareSerial dfSerial(2);
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
MFRC522::MIFARE_Key rfidKey;
WiFiServer logServer(LOG_STREAM_PORT);
WiFiClient logClient;
bool dfSerialReady = false;
bool otaReady = false;
bool playbackStarted = false;
int lastVolume = -1;
int pendingVolume = -1;
int filteredVolumeRaw = -1;
int lastAcceptedVolumeRaw = -1;
int volumeRawCandidate = -1;
unsigned long playbackDueAt = 0;
unsigned long lastPlayAttemptAt = 0;
unsigned long lastVolumePollAt = 0;
unsigned long lastRfidPollAt = 0;
unsigned long lastRfidCardLogAt = 0;
unsigned long pendingVolumeChangedAt = 0;
unsigned long volumeRawCandidateSince = 0;
unsigned long gpio32RawChangedAt = 0;
unsigned long btnForwardRawChangedAt = 0;
unsigned long btnBackRawChangedAt = 0;
unsigned long timerButtonRawChangedAt = 0;
unsigned long timerButtonPressedAt = 0;
uint8_t playAttempts = 0;
unsigned long lastLedToggleAt = 0;
bool statusLedOn = false;
bool gpio32RawState = HIGH;
bool gpio32StableState = HIGH;
bool btnForwardRawState = HIGH;
bool btnForwardStableState = HIGH;
bool btnBackRawState = HIGH;
bool btnBackStableState = HIGH;
bool timerButtonRawState = HIGH;
bool timerButtonStableState = HIGH;
bool gpioPlaybackAllowed = false;
bool playbackEverStarted = false;
bool playbackPaused = false;
uint16_t testTimerMinutes = 0;
byte rfidReaderVersion = 0;
String lastRfidUid = "";
uint8_t dfFeedbackPacket[10];
uint8_t dfFeedbackIndex = 0;
unsigned long dfFeedbackStartedAt = 0;

void setupStatusLed() {
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
}

void updateStatusLed() {
  unsigned long now = millis();
  if (static_cast<unsigned long>(now - lastLedToggleAt) < LED_BLINK_MS) {
    return;
  }

  lastLedToggleAt = now;
  statusLedOn = !statusLedOn;
  digitalWrite(STATUS_LED_PIN, statusLedOn ? HIGH : LOW);
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
    logClient.println("[LOG] DFPlayer-Test WLAN-Konsole verbunden");
  }
}

void logLine(const String& text) {
  Serial.println(text);
  updateLogClient();
  if (logClient && logClient.connected()) {
    logClient.println(text);
  }
}

String pinStateText(uint8_t pin) {
  return digitalRead(pin) == LOW ? "GND" : "offen";
}

String hexByte(uint8_t value) {
  String text = String(value, HEX);
  text.toUpperCase();
  if (text.length() < 2) {
    text = "0" + text;
  }
  return text;
}

String hexWord(uint16_t value) {
  String text = String(value, HEX);
  text.toUpperCase();
  while (text.length() < 4) {
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

String rfidVersionText(byte version) {
  return "0x" + hexByte(version);
}

bool rfidReaderConnected() {
  return rfidReaderVersion != 0x00 && rfidReaderVersion != 0xFF;
}

bool authenticateClassicBlock(byte blockAddr, byte trailerBlock) {
  struct AuthAttempt {
    byte command;
    byte block;
    const char* label;
  };

  AuthAttempt attempts[] = {
    {MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, "Key A Trailer 7"},
    {MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockAddr, "Key A Block 4"},
    {MFRC522::PICC_CMD_MF_AUTH_KEY_B, trailerBlock, "Key B Trailer 7"},
    {MFRC522::PICC_CMD_MF_AUTH_KEY_B, blockAddr, "Key B Block 4"},
  };

  for (const auto& attempt : attempts) {
    MFRC522::StatusCode status = rfid.PCD_Authenticate(
      attempt.command,
      attempt.block,
      &rfidKey,
      &(rfid.uid)
    );

    if (status == MFRC522::STATUS_OK) {
      logLine("[RFIDTEST] Classic Auth OK: " + String(attempt.label));
      return true;
    }

    rfid.PCD_StopCrypto1();
  }

  logLine("[RFIDTEST] Classic Auth Fehler: alle Standardversuche fehlgeschlagen");
  return false;
}

bool readTonuinoRawData(byte* data, byte dataLength) {
  if (dataLength < 18) {
    return false;
  }

  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  if (piccType == MFRC522::PICC_TYPE_MIFARE_1K ||
      piccType == MFRC522::PICC_TYPE_MIFARE_4K) {
    byte blockAddr = 4;
    byte trailerBlock = 7;
    byte size = dataLength;

    if (!authenticateClassicBlock(blockAddr, trailerBlock)) {
      return false;
    }

    MFRC522::StatusCode status = rfid.MIFARE_Read(blockAddr, data, &size);
    if (status != MFRC522::STATUS_OK) {
      logLine("[RFIDTEST] Classic Block 4 Read Fehler: " +
              String(rfid.GetStatusCodeName(status)));
      return false;
    }

    logLine("[RFIDTEST] Classic Block 4 RAW " + bytesToHexLine(data, 16));
    return true;
  }

  if (piccType == MFRC522::PICC_TYPE_MIFARE_UL) {
    for (byte page = 4; page <= 40; page += 4) {
      byte buffer[18] = {};
      byte size = sizeof(buffer);
      MFRC522::StatusCode status = rfid.MIFARE_Read(page, buffer, &size);

      if (status != MFRC522::STATUS_OK) {
        continue;
      }

      bool validCookie = buffer[0] == 0x13 &&
                         buffer[1] == 0x37 &&
                         buffer[2] == 0xB3 &&
                         buffer[3] == 0x47;
      if (validCookie) {
        memcpy(data, buffer, dataLength);
        logLine("[RFIDTEST] UL p" + String(page) + "-" + String(page + 3) +
                " TonUINO RAW " + bytesToHexLine(buffer, 16));
        return true;
      }
    }

    logLine("[RFIDTEST] UL TonUINO Cookie nicht gefunden");
    return false;
  }

  logLine("[RFIDTEST] Tag-Typ fuer TonUINO RAW nicht unterstuetzt");
  return false;
}

void logTonuinoDecode(const byte* data) {
  bool validCookie = data[0] == 0x13 &&
                     data[1] == 0x37 &&
                     data[2] == 0xB3 &&
                     data[3] == 0x47;
  if (!validCookie) {
    logLine("[RFIDTEST] TonUINO Cookie fehlt");
    return;
  }

  logLine("[RFIDTEST] TonUINO Cookie OK Version=" + String(data[4]) +
          " Folder=" + String(data[5]) +
          " Mode=" + String(data[6]) +
          " Special=" + String(data[7]) +
          " Special2=" + String(data[8]));
}

void setupRfidDiagnostics() {
  SPI.begin(RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN, RFID_SS_PIN);
  rfid.PCD_Init();
  delay(50);
  rfid.PCD_AntennaOn();
  rfid.PCD_SetAntennaGain(rfid.RxGain_max);
  rfidReaderVersion = rfid.PCD_ReadRegister(MFRC522::VersionReg);

  for (byte i = 0; i < 6; i++) {
    rfidKey.keyByte[i] = 0xFF;
  }

  logLine("[RFIDTEST] Pins SS=5 RST=22 SCK=18 MISO=19 MOSI=23");
  logLine("[RFIDTEST] RC522 Version " + rfidVersionText(rfidReaderVersion) +
          String(rfidReaderConnected() ? " erkannt" : " NICHT erreichbar"));
  logLine("[RFIDTEST] Selftest " + String(rfid.PCD_PerformSelfTest() ? "OK" : "FEHLER"));
  rfid.PCD_Init();
  delay(50);
  rfid.PCD_AntennaOn();
  rfid.PCD_SetAntennaGain(rfid.RxGain_max);
}

void updateRfidDiagnostics() {
  unsigned long now = millis();
  if (static_cast<unsigned long>(now - lastRfidPollAt) < RFID_POLL_MS) {
    return;
  }
  lastRfidPollAt = now;

  rfid.PCD_StopCrypto1();
  bool selected = rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial();

  if (!selected) {
    byte atqa[2] = {};
    byte atqaSize = sizeof(atqa);
    MFRC522::StatusCode status = rfid.PICC_WakeupA(atqa, &atqaSize);
    selected = status == MFRC522::STATUS_OK && rfid.PICC_ReadCardSerial();
  }

  if (!selected) {
    return;
  }

  String uid = uidToString(&rfid.uid);
  bool repeatReady = static_cast<unsigned long>(now - lastRfidCardLogAt) >= RFID_REPEAT_LOG_MS;
  if (uid == lastRfidUid && !repeatReady) {
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;
  }

  lastRfidUid = uid;
  lastRfidCardLogAt = now;
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  logLine("[RFIDTEST] Karte UID=" + uid +
          " Type=" + String(rfid.PICC_GetTypeName(piccType)) +
          " SAK=0x" + hexByte(rfid.uid.sak));

  byte rawData[18] = {};
  if (readTonuinoRawData(rawData, sizeof(rawData))) {
    logTonuinoDecode(rawData);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

String dfFeedbackTypeText(uint8_t type, uint16_t value) {
  switch (type) {
    case 0x3A: return "Medium gesteckt";
    case 0x3B: return "Medium entfernt";
    case 0x3C: return "USB Track beendet " + String(value);
    case 0x3D: return "SD Track beendet " + String(value);
    case 0x3E: return "Flash Track beendet " + String(value);
    case 0x3F: return "Initialisierung " + String(value);
    case 0x40: return "Fehler " + String(value);
    case 0x41: return "ACK fuer Kommando";
    case 0x42: return "Status " + String(value);
    case 0x43: return "Lautstaerke " + String(value);
    case 0x44: return "Equalizer " + String(value);
    case 0x45: return "Playback Mode " + String(value);
    case 0x46: return "Software-Version " + String(value);
    case 0x47: return "USB Dateien " + String(value);
    case 0x48: return "SD Dateien " + String(value);
    case 0x49: return "Flash Dateien " + String(value);
    case 0x4B: return "USB aktueller Track " + String(value);
    case 0x4C: return "SD aktueller Track " + String(value);
    case 0x4D: return "Flash aktueller Track " + String(value);
    case 0x4E: return "Ordner Dateien " + String(value);
    case 0x4F: return "Ordner Anzahl " + String(value);
    default: return "Typ 0x" + hexByte(type) + " Wert " + String(value);
  }
}

void logDfFeedbackFrame(const uint8_t* packet) {
  String raw = "";
  for (uint8_t i = 0; i < 10; i++) {
    if (i > 0) {
      raw += " ";
    }
    raw += hexByte(packet[i]);
  }

  if (packet[0] != 0x7E || packet[1] != 0xFF || packet[2] != 0x06 || packet[9] != 0xEF) {
    logLine("[DFRX] Ungueltiges Frame: " + raw);
    return;
  }

  uint16_t sum = 0;
  for (uint8_t i = 1; i <= 6; i++) {
    sum += packet[i];
  }
  uint16_t expectedChecksum = 0 - sum;
  uint16_t receivedChecksum = (static_cast<uint16_t>(packet[7]) << 8) | packet[8];
  uint8_t type = packet[3];
  uint16_t value = (static_cast<uint16_t>(packet[5]) << 8) | packet[6];

  String checksumText = expectedChecksum == receivedChecksum
                            ? "Checksum OK"
                            : "Checksum FEHLER erwartet 0x" + hexWord(expectedChecksum);
  logLine("[DFRX] " + dfFeedbackTypeText(type, value) +
          " | Wert=0x" + hexWord(value) +
          " | " + checksumText +
          " | RAW " + raw);
}

void updateDfFeedback() {
  unsigned long now = millis();
  if (dfFeedbackIndex > 0 &&
      static_cast<unsigned long>(now - dfFeedbackStartedAt) > DF_FEEDBACK_FRAME_TIMEOUT_MS) {
    logLine("[DFRX] Unvollstaendiges Frame verworfen (" + String(dfFeedbackIndex) + " Bytes)");
    dfFeedbackIndex = 0;
  }

  while (dfSerial.available() > 0) {
    uint8_t value = static_cast<uint8_t>(dfSerial.read());

    if (dfFeedbackIndex == 0) {
      if (value != 0x7E) {
        logLine("[DFRX] Einzelbyte ohne Frame 0x" + hexByte(value));
        continue;
      }
      dfFeedbackStartedAt = now;
    }

    dfFeedbackPacket[dfFeedbackIndex++] = value;

    if (dfFeedbackIndex < sizeof(dfFeedbackPacket)) {
      continue;
    }

    logDfFeedbackFrame(dfFeedbackPacket);
    dfFeedbackIndex = 0;
  }
}

void sendDfCommand(uint8_t command, uint16_t argument) {
  uint8_t packet[10] = {
    0x7E, 0xFF, 0x06, command, DF_REQUEST_FEEDBACK ? 0x01 : 0x00,
    static_cast<uint8_t>(argument >> 8),
    static_cast<uint8_t>(argument & 0xFF),
    0x00, 0x00, 0xEF
  };

  uint16_t sum = 0;
  for (uint8_t i = 1; i <= 6; i++) {
    sum += packet[i];
  }
  uint16_t checksum = 0 - sum;
  packet[7] = static_cast<uint8_t>(checksum >> 8);
  packet[8] = static_cast<uint8_t>(checksum & 0xFF);

  dfSerial.write(packet, sizeof(packet));
  dfSerial.flush();
}

int readMedianRaw() {
  constexpr int SAMPLES = 7;
  int samples[SAMPLES];

  for (int i = 0; i < SAMPLES; i++) {
    samples[i] = analogRead(VOL_PIN);
    delayMicroseconds(300);
  }

  for (int i = 1; i < SAMPLES; i++) {
    int value = samples[i];
    int j = i - 1;
    while (j >= 0 && samples[j] > value) {
      samples[j + 1] = samples[j];
      j--;
    }
    samples[j + 1] = value;
  }

  return samples[SAMPLES / 2];
}

int stabilizeVolumeRaw(int raw, unsigned long now) {
  if (lastAcceptedVolumeRaw < 0) {
    lastAcceptedVolumeRaw = raw;
    volumeRawCandidate = raw;
    volumeRawCandidateSince = now;
    return raw;
  }

  bool rawAtRail = raw <= VOL_RAIL_LOW || raw >= VOL_RAIL_HIGH;
  bool acceptedAtRail = lastAcceptedVolumeRaw <= VOL_RAIL_LOW || lastAcceptedVolumeRaw >= VOL_RAIL_HIGH;
  bool suspiciousRailJump = rawAtRail && !acceptedAtRail && abs(raw - lastAcceptedVolumeRaw) >= VOL_GLITCH_JUMP;

  if (!suspiciousRailJump) {
    lastAcceptedVolumeRaw = raw;
    volumeRawCandidate = raw;
    volumeRawCandidateSince = now;
    return raw;
  }

  if (abs(raw - volumeRawCandidate) > VOL_RAW_LOG_DELTA) {
    volumeRawCandidate = raw;
    volumeRawCandidateSince = now;
    return lastAcceptedVolumeRaw;
  }

  if (static_cast<unsigned long>(now - volumeRawCandidateSince) >= VOL_RAIL_STABLE_MS) {
    lastAcceptedVolumeRaw = raw;
    return raw;
  }

  return lastAcceptedVolumeRaw;
}

int rawToVolume(int raw) {
  raw = constrain(raw, 0, 4095);

  constexpr int thresholds[VOL_MAX] = {
    199, 350, 550, 850, 1200, 1650, 2150, 2700, 3300, 3900
  };

  for (int volume = 0; volume < VOL_MAX; volume++) {
    if (raw <= thresholds[volume]) {
      return volume;
    }
  }
  return VOL_MAX;
}

void updateGpio32Test() {
  unsigned long now = millis();
  bool rawState = digitalRead(GPIO32_TEST_PIN);

  if (rawState != gpio32RawState) {
    gpio32RawState = rawState;
    gpio32RawChangedAt = now;
  }

  if (rawState == gpio32StableState ||
      static_cast<unsigned long>(now - gpio32RawChangedAt) < GPIO32_DEBOUNCE_MS) {
    return;
  }

  gpio32StableState = rawState;
  if (gpio32StableState == LOW) {
    gpioPlaybackAllowed = true;
    logLine("[GPIO] 32");
    if (playbackEverStarted && playbackPaused) {
      sendDfCommand(0x0D, 0x0000);  // Play/Resume
      playbackPaused = false;
      return;
    }

    if (!playbackEverStarted) {
      playbackDueAt = millis();
    }
    return;
  }

  gpioPlaybackAllowed = false;
  if (playbackEverStarted && !playbackPaused) {
    sendDfCommand(0x0E, 0x0000);    // Pause
    playbackPaused = true;
  }
}

void setupTrackButtons() {
  pinMode(BTN_FROWARD_PIN, INPUT_PULLUP);
  btnForwardRawState = digitalRead(BTN_FROWARD_PIN);
  btnForwardStableState = btnForwardRawState;
  btnForwardRawChangedAt = millis();

  pinMode(BTN_BACK_PIN, INPUT_PULLUP);
  btnBackRawState = digitalRead(BTN_BACK_PIN);
  btnBackStableState = btnBackRawState;
  btnBackRawChangedAt = millis();
}

void updateBtnForward() {
  unsigned long now = millis();
  bool rawState = digitalRead(BTN_FROWARD_PIN);

  if (rawState != btnForwardRawState) {
    btnForwardRawState = rawState;
    btnForwardRawChangedAt = now;
  }

  if (rawState == btnForwardStableState ||
      static_cast<unsigned long>(now - btnForwardRawChangedAt) < TRACK_BUTTON_DEBOUNCE_MS) {
    return;
  }

  btnForwardStableState = rawState;
  if (btnForwardStableState == LOW) {
    logLine("[GPIO] " + String(BTN_FROWARD_PIN));
    sendDfCommand(0x01, 0x0000);    // Next
    playbackStarted = true;
    playbackEverStarted = true;
    playbackPaused = false;
  }
}

void updateBtnBack() {
  unsigned long now = millis();
  bool rawState = digitalRead(BTN_BACK_PIN);

  if (rawState != btnBackRawState) {
    btnBackRawState = rawState;
    btnBackRawChangedAt = now;
  }

  if (rawState == btnBackStableState ||
      static_cast<unsigned long>(now - btnBackRawChangedAt) < TRACK_BUTTON_DEBOUNCE_MS) {
    return;
  }

  btnBackStableState = rawState;
  if (btnBackStableState == LOW) {
    logLine("[GPIO] " + String(BTN_BACK_PIN));
    sendDfCommand(0x02, 0x0000);    // Previous
    playbackStarted = true;
    playbackEverStarted = true;
    playbackPaused = false;
  }
}

void setupTimerButton() {
  pinMode(TIMER_BUTTON_PIN, INPUT_PULLUP);
  timerButtonRawState = digitalRead(TIMER_BUTTON_PIN);
  timerButtonStableState = timerButtonRawState;
  timerButtonRawChangedAt = millis();
  timerButtonPressedAt = timerButtonStableState == LOW ? millis() : 0;
}

void updateTimerButton() {
  unsigned long now = millis();
  bool rawState = digitalRead(TIMER_BUTTON_PIN);

  if (rawState != timerButtonRawState) {
    timerButtonRawState = rawState;
    timerButtonRawChangedAt = now;
  }

  if (rawState == timerButtonStableState ||
      static_cast<unsigned long>(now - timerButtonRawChangedAt) < TIMER_BUTTON_DEBOUNCE_MS) {
    return;
  }

  timerButtonStableState = rawState;
  if (timerButtonStableState == LOW) {
    timerButtonPressedAt = now;
    logLine("[GPIO] 33");
    return;
  }

  unsigned long pressedMs = static_cast<unsigned long>(now - timerButtonPressedAt);
  if (pressedMs >= TIMER_BUTTON_LONG_PRESS_MS) {
    testTimerMinutes = 0;
    return;
  }

  testTimerMinutes += TIMER_BUTTON_STEP_MINUTES;
}

void sendVolume(uint8_t volume) {
  uint8_t dfVolume = volume == 0
                         ? 0
                         : static_cast<uint8_t>(map(constrain(volume, 1, VOL_MAX),
                                                    1, VOL_MAX,
                                                    VOL_MIN_AUDIBLE, DF_VOLUME_MAX));
  sendDfCommand(0x06, dfVolume);
}

void logVolumeState(const char* prefix, int volume, uint8_t dfVolume, int raw, int stableRaw) {
  logLine(String(prefix) +
          " Regler " + String(volume) + "/" + String(VOL_MAX) +
          " DF " + String(dfVolume) +
          " RAW=" + String(raw) +
          " STABLE=" + String(stableRaw) +
          " FILT=" + String(filteredVolumeRaw));
}

void updateVolume() {
  unsigned long now = millis();
  if (static_cast<unsigned long>(now - lastVolumePollAt) < VOL_POLL_MS) {
    return;
  }
  lastVolumePollAt = now;

  int raw = readMedianRaw();
  int stableRaw = stabilizeVolumeRaw(raw, now);

  filteredVolumeRaw = stableRaw;

  int volume = rawToVolume(filteredVolumeRaw);
  uint8_t dfVolume = volume == 0
                         ? 0
                         : static_cast<uint8_t>(map(constrain(volume, 1, VOL_MAX),
                                                    1, VOL_MAX,
                                                    VOL_MIN_AUDIBLE, DF_VOLUME_MAX));

  if (volume == lastVolume) {
    pendingVolume = volume;
    pendingVolumeChangedAt = now;
    return;
  }

  if (volume != pendingVolume) {
    pendingVolume = volume;
    pendingVolumeChangedAt = now;
  }

  bool largeChange = lastVolume < 0 || abs(volume - lastVolume) >= 2;
  bool stableStep = static_cast<unsigned long>(now - pendingVolumeChangedAt) >= VOL_STEP_STABLE_MS;
  if (!largeChange && !stableStep) {
    return;
  }

  lastVolume = volume;
  sendVolume(static_cast<uint8_t>(volume));
  logVolumeState("[VOLTEST] Lautstaerke geaendert", volume, dfVolume, raw, stableRaw);
}

void startPlayback() {
  if (!gpioPlaybackAllowed) {
    return;
  }

  logLine("[DFTEST] Sende rohe DFPlayer-Kommandos " + String(playAttempts + 1) + "/" + String(PLAY_ATTEMPTS));
  logLine("[DFTEST] Erwartet z.B. /0001.mp3 oder erste Datei auf SD");
  sendDfCommand(0x09, 0x0002);      // SD-Karte auswaehlen
  delay(100);
  sendVolume(static_cast<uint8_t>(max(lastVolume, 0)));
  delay(100);
  sendDfCommand(0x03, 0x0001);      // Datei 1 abspielen
  lastPlayAttemptAt = millis();
  playAttempts++;
  playbackStarted = true;
  playbackEverStarted = true;
  playbackPaused = false;
}

void updatePlaybackStart() {
  if (!dfSerialReady || !gpioPlaybackAllowed || playAttempts >= PLAY_ATTEMPTS) {
    return;
  }

  unsigned long now = millis();
  if (playAttempts == 0 && static_cast<long>(now - playbackDueAt) < 0) {
    return;
  }

  if (playAttempts > 0 &&
      static_cast<unsigned long>(now - lastPlayAttemptAt) < PLAY_RETRY_MS) {
    return;
  }

  startPlayback();
}

void setupOta() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(OTA_NAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  logLine("[OTA] Verbinde WLAN...");
  unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         static_cast<unsigned long>(millis() - startedAt) < WIFI_CONNECT_TIMEOUT_MS) {
    updateStatusLed();
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    logLine("[OTA] WLAN nicht verbunden, OTA aus");
    return;
  }

  ArduinoOTA.setHostname(OTA_NAME);
  ArduinoOTA.onStart([]() {
    logLine("[OTA] Update gestartet");
  });
  ArduinoOTA.onEnd([]() {
    logLine("[OTA] Update beendet");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if (total == 0) {
      return;
    }
    logLine("[OTA] Fortschritt " + String((progress * 100U) / total) + "%");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    logLine("[OTA] Fehler " + String(static_cast<int>(error)));
  });
  ArduinoOTA.begin();
  logServer.begin();
  otaReady = true;

  logLine("[OTA] Bereit: " + String(OTA_NAME) + " @ " + WiFi.localIP().toString());
  logLine("[LOG] WLAN-Konsole: nc " + WiFi.localIP().toString() + " " + String(LOG_STREAM_PORT));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  setupStatusLed();
  setupTrackButtons();
  setupTimerButton();
  pinMode(GPIO32_TEST_PIN, INPUT_PULLUP);
  gpio32RawState = digitalRead(GPIO32_TEST_PIN);
  gpio32StableState = gpio32RawState;
  gpio32RawChangedAt = millis();
  analogReadResolution(12);
  analogSetPinAttenuation(VOL_PIN, ADC_11db);
  setupOta();
  delay(DF_POWERUP_DELAY_MS);

  dfSerial.begin(9600, SERIAL_8N1, DF_RX_PIN, DF_TX_PIN);
  dfSerialReady = true;
  delay(500);

  int initialRaw = readMedianRaw();
  lastAcceptedVolumeRaw = initialRaw;
  volumeRawCandidate = initialRaw;
  volumeRawCandidateSince = millis();
  filteredVolumeRaw = initialRaw;
  lastVolume = rawToVolume(initialRaw);
  pendingVolume = lastVolume;
  pendingVolumeChangedAt = millis();
  lastVolumePollAt = millis();
  sendVolume(static_cast<uint8_t>(lastVolume));
  gpioPlaybackAllowed = gpio32StableState == LOW;
  if (gpioPlaybackAllowed) {
    playbackDueAt = millis() + PLAY_START_DELAY_MS;
  }
}

void loop() {
  if (otaReady) {
    ArduinoOTA.handle();
    updateLogClient();
  }

  if (!dfSerialReady) {
    updateStatusLed();
    delay(20);
    return;
  }

  updateStatusLed();
  updateBtnForward();
  updateBtnBack();
  updateTimerButton();
  updateGpio32Test();
  updateVolume();
}
