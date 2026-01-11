#pragma once
#include <stdint.h>

#define ROLE_CONTROLLER 1

// Debug/logging: 1 enables Serial logs, 0 disables.
#define PERF_DEBUG 0

// Footer with time/key on main screens: 1 enables, 0 disables.
#define FOOTER_TIMEKEY_ENABLE 1

// ===== Startup screen =====
// StartScreen::DefaultSplash   -> as before: splash + loop_main page 1
// StartScreen::DirectMain      -> skip splash, start on loop_main page 1
// StartScreen::DirectSetExpo   -> jump directly to EXPO screen (useful for tests)
// StartScreen::DirectCalibJoy  -> start in joystick calibration screen
// StartScreen::DirectDeadzone  -> start in deadzone screen
// StartScreen::DirectKeysThr   -> start in key-threshold calibration screen
enum class StartScreen : uint8_t
{
    DefaultSplash = 0,
    DirectMain,
    DirectSetExpo,
    DirectCalibJoy,
    DirectDeadzone,
    DirectKeysThr
};

// Set startup screen (default: splash + loop_main page 1)
#define START_SCREEN StartScreen::DefaultSplash

// ===== GPIO / pins =====
#define BUTTONS_KEY_PIN A7
#define JOY_L_PIN_X A1
#define JOY_L_PIN_Y A2
#define JOY_L_PIN_BTN 23
#define JOY_R_PIN_X A3
#define JOY_R_PIN_Y A4
#define JOY_R_PIN_BTN 22

// ===== Joysticks =====
#define JOY_DEADZONE_DEFAULT 40
#define JOY_EXPO_DEFAULT 1.8f

// ===== Analog keyboard thresholds (0..1023) =====
#define TH_DOWN_DEFAULT 550
#define TH_UP_DEFAULT 625
#define TH_RIGHT_DEFAULT 700
#define TH_CENTER_DEFAULT 800
#define TH_LEFT_DEFAULT 875

// ===== Display / OLED =====
#define DISPLAY_MIN_FLUSH_INTERVAL_MS 50

// ===== UART / comms =====
#define COMM_BAUD 115200

// ===== NRF24 (SPI) =====
// Mega2560 hardware SPI pins: SCK=52, MOSI=51, MISO=50.
#define NRF_CSN_PIN 8
#define NRF_CE_PIN 7
#define NRF_SCK_PIN 52
#define NRF_MOSI_PIN 51
#define NRF_MISO_PIN 50
#define NRF_CHANNEL 76
#define NRF_PA_LEVEL 0 // RF24_PA_MIN (range: 0=MIN .. 3=MAX)

// ===== EEPROM =====
// Set to 1 to force writing defaults to EEPROM on boot (use once, then set back to 0).
#define EEPROM_FORCE_DEFAULTS_ON_BOOT 0
