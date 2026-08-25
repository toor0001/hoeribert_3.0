#pragma once

#include <Arduino.h>
#include <MFRC522.h>
#include "HardwarePins.h"

struct TonuinoCardData {
  bool valid = false;
  uint8_t version = 0;
  uint8_t folder = 0;
  uint8_t mode = 0;
  uint8_t special = 0;
  uint8_t special2 = 0;
  String uid = "";
  String cardType = "";
};

struct CardBookmark {
  bool valid = false;
  uint8_t folder = 0;
  uint8_t track = 0;
  uint16_t seconds = 0;
};

class RFIDManager {
public:
  void begin();
  bool update();
  bool isCardPresent() const;
  TonuinoCardData readTonuinoCard() const;
  String getLastUid() const;
  String getLastCardType() const;
  String getLastRawData() const;
  String getLastError() const;
  String getReaderVersionText() const;
  String getRxGainText() const;
  bool isRxGainMaximum() const;
  bool isCardRecentlyDetected() const;
  bool hasDetectionQuality() const;
  uint8_t getDetectionQualityPercent() const;
  uint8_t getDetectionQualitySuccesses() const;
  uint8_t getDetectionQualityAttempts() const;
  bool isReaderConnected() const;
  void powerDown();
  bool hasLastRawData() const;
  void copyLastRawData(byte* data, size_t maxLength) const;
  int getDebugLineCount() const;
  String getDebugLine(int index) const;

private:
  static constexpr uint8_t RFID_SS_PIN   = HardwarePins::RFID_SS;
  static constexpr uint8_t RFID_RST_PIN  = HardwarePins::RFID_RST;
  static constexpr uint8_t RFID_SCK_PIN  = HardwarePins::RFID_SCK;
  static constexpr uint8_t RFID_MISO_PIN = HardwarePins::RFID_MISO;
  static constexpr uint8_t RFID_MOSI_PIN = HardwarePins::RFID_MOSI;
  static constexpr int RAW_DATA_LENGTH = 16;
  static constexpr int BUFFER_LENGTH = 18;
  static constexpr int MAX_DEBUG_LINES = 16;
  static constexpr uint8_t QUALITY_WINDOW_SIZE = 50;
  static constexpr unsigned long CARD_RECENT_MS = 3000;

  bool selectCard(uint8_t attempts = 1);
  bool readTonuinoRawData(byte* data);
  bool authenticateClassicBlock(byte blockAddr, byte trailerBlock);
  TonuinoCardData decodeTonuinoCard(const byte* data) const;
  String uidToString(MFRC522::Uid* uid) const;
  void clearDebugLines();
  void addDebugLine(const String& line);
  String bytesToHexLine(const byte* data, int length) const;
  void finishCard();
  void recordDetectionResult(bool success, unsigned long now);
  void resetDetectionQuality();
  static uint8_t rxGainDb(byte gain);

  MFRC522 rfid{RFID_SS_PIN, RFID_RST_PIN};
  MFRC522::MIFARE_Key rfidKey;
  byte readerVersion = 0;
  byte configuredRxGain = 0;
  String lastUid = "";
  String lastReportedUid = "";
  String lastCardType = "";
  String lastRawData = "";
  String lastError = "";
  unsigned long lastUidTime = 0;
  bool cardPresent = false;
  bool lastRawDataAvailable = false;
  TonuinoCardData lastTonuinoCard;
  byte lastRawBytes[BUFFER_LENGTH] = {};
  String debugLines[MAX_DEBUG_LINES];
  int debugLineCount = 0;
  bool qualityWindow[QUALITY_WINDOW_SIZE] = {};
  uint8_t qualityWindowCount = 0;
  uint8_t qualityWindowIndex = 0;
  uint8_t qualitySuccessCount = 0;
  unsigned long lastCardSeenAt = 0;
};
