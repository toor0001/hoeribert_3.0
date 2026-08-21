#pragma once

#include <Arduino.h>

namespace HardwarePins {
constexpr uint8_t VOLUME = 34;
constexpr uint8_t PLAY_BUTTON = 26;
constexpr uint8_t FORWARD_BUTTON = 14;
constexpr uint8_t BACK_BUTTON = 13;
constexpr uint8_t TIMER_BUTTON = 25;
constexpr uint8_t STATUS_LED = 32;

constexpr uint8_t RFID_SS = 5;
constexpr uint8_t RFID_RST = 22;
constexpr uint8_t RFID_SCK = 18;
constexpr uint8_t RFID_MISO = 19;
constexpr uint8_t RFID_MOSI = 23;

constexpr uint8_t DFPLAYER_RX = 16;
constexpr uint8_t DFPLAYER_TX = 17;
}
