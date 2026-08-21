#include <Arduino.h>
#include <MFRC522.h>
#include <SPI.h>
#include "hardware/HardwarePins.h"

static constexpr uint8_t RFID_SS_PIN = HardwarePins::RFID_SS;
static constexpr uint8_t RFID_RST_PIN = HardwarePins::RFID_RST;
static constexpr uint8_t RFID_SCK_PIN = HardwarePins::RFID_SCK;
static constexpr uint8_t RFID_MISO_PIN = HardwarePins::RFID_MISO;
static constexpr uint8_t RFID_MOSI_PIN = HardwarePins::RFID_MOSI;
static constexpr bool FULL_CARD_DUMP = false;

MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);
unsigned long lastIdleLogAt = 0;

void printHexBytes(const byte* data, byte length) {
  for (byte i = 0; i < length; i++) {
    if (data[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(data[i], HEX);
    Serial.print(' ');
  }
}

void dumpUltralightPages() {
  Serial.println(F("[DumpInfo] Ultralight pages, je 16 Bytes ab Startpage:"));

  for (byte page = 4; page <= 40; page += 4) {
    byte buffer[18] = {};
    byte size = sizeof(buffer);
    MFRC522::StatusCode status = mfrc522.MIFARE_Read(page, buffer, &size);

    Serial.print(F("[DumpInfo] UL p"));
    Serial.print(page);
    Serial.print(F("-"));
    Serial.print(page + 3);
    Serial.print(F(": "));

    if (status != MFRC522::STATUS_OK) {
      Serial.println(mfrc522.GetStatusCodeName(status));
      continue;
    }

    printHexBytes(buffer, 16);

    if (buffer[0] == 0x13 && buffer[1] == 0x37 &&
        buffer[2] == 0xB3 && buffer[3] == 0x47) {
      Serial.print(F(" TonUINO cookie"));
    }

    Serial.println();
  }
}

void logIdleStatus() {
  unsigned long now = millis();
  if (now - lastIdleLogAt < 1000) {
    return;
  }
  lastIdleLogAt = now;

  byte version = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
  byte atqa[2] = {};
  byte atqaSize = sizeof(atqa);
  MFRC522::StatusCode status = mfrc522.PICC_WakeupA(atqa, &atqaSize);

  Serial.print(F("[DumpInfo] Suche Karte... RC522=0x"));
  if (version < 0x10) {
    Serial.print('0');
  }
  Serial.print(version, HEX);
  Serial.print(F(" WakeupA="));
  Serial.print(mfrc522.GetStatusCodeName(status));

  if (status == MFRC522::STATUS_OK) {
    Serial.print(F(" ATQA="));
    for (byte i = 0; i < atqaSize; i++) {
      if (atqa[i] < 0x10) {
        Serial.print('0');
      }
      Serial.print(atqa[i], HEX);
      Serial.print(' ');
    }
  }

  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println(F("[DumpInfo] MFRC522 original dump test"));
  Serial.println(F("[DumpInfo] Pins: SS=5 RST=22 SCK=18 MISO=19 MOSI=23"));

  SPI.begin(RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN, RFID_SS_PIN);
  mfrc522.PCD_Init();
  delay(50);
  mfrc522.PCD_AntennaOn();
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  mfrc522.PCD_DumpVersionToSerial();
  Serial.print(F("[DumpInfo] Self test: "));
  Serial.println(mfrc522.PCD_PerformSelfTest() ? F("OK") : F("FEHLER"));
  mfrc522.PCD_Init();
  delay(50);
  mfrc522.PCD_AntennaOn();
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
  Serial.println(F("[DumpInfo] Scan PICC to see UID, SAK, and type..."));
  if (FULL_CARD_DUMP) {
    Serial.println(F("[DumpInfo] Full card dump enabled"));
  }
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent()) {
    logIdleStatus();
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial()) {
    Serial.println(F("[DumpInfo] Karte gesehen, aber PICC_ReadCardSerial() fehlgeschlagen"));
    return;
  }

  Serial.print(F("[DumpInfo] Card UID:"));
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? F(" 0") : F(" "));
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();

  Serial.print(F("[DumpInfo] Card SAK: "));
  Serial.println(mfrc522.uid.sak, HEX);

  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  Serial.print(F("[DumpInfo] PICC type: "));
  Serial.println(mfrc522.PICC_GetTypeName(piccType));

  if (piccType == MFRC522::PICC_TYPE_MIFARE_UL) {
    dumpUltralightPages();
  }

  if (FULL_CARD_DUMP) {
    mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(500);
}
