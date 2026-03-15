#pragma once
#include <stdint.h>
#include "controller/hardware_config.h"

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
enum class StartScreen : uint8_t
{
    DefaultSplash = 0,
    DirectMain,
    DirectSetExpo,
    DirectCalibJoy,
    DirectDeadzone
};

// Set startup screen (default: splash + loop_main page 1)
#define START_SCREEN StartScreen::DefaultSplash

// ===== GPIO / pins =====
#define JOY_L_PIN_X HW_JOY_L_PIN_X
#define JOY_L_PIN_Y HW_JOY_L_PIN_Y
#define JOY_L_PIN_BTN HW_JOY_L_PIN_BTN
#define JOY_R_PIN_X HW_JOY_R_PIN_X
#define JOY_R_PIN_Y HW_JOY_R_PIN_Y
#define JOY_R_PIN_BTN HW_JOY_R_PIN_BTN

#define PHOTO_PIN HW_PHOTO_PIN
#define BATTERY_PIN HW_BATTERY_PIN

#define BUTTON_UP_PIN HW_BTN_UP_PIN
#define BUTTON_LEFT_PIN HW_BTN_LEFT_PIN
#define BUTTON_CENTER_PIN HW_BTN_CENTER_PIN
#define BUTTON_RIGHT_PIN HW_BTN_RIGHT_PIN
#define BUTTON_DOWN_PIN HW_BTN_DOWN_PIN
#define BUTTON_F1_PIN HW_BTN_F1_PIN
#define BUTTON_F2_PIN HW_BTN_F2_PIN

// ===== Joysticks =====
#define JOY_DEADZONE_DEFAULT 160
#define JOY_EXPO_DEFAULT 1.8f

// ===== Legacy keyboard thresholds =====
// Kept temporarily for the migration period. Digital button handling
// will replace this in a later step.
#define TH_DOWN_DEFAULT 550
#define TH_UP_DEFAULT 625
#define TH_RIGHT_DEFAULT 700
#define TH_CENTER_DEFAULT 800
#define TH_LEFT_DEFAULT 875

// ===== Display / OLED =====
#define DISPLAY_MIN_FLUSH_INTERVAL_MS 50
#define DISPLAY_UI_REFRESH_INTERVAL_MS 150
#define I2C_SCL_PIN HW_I2C_SCL_PIN
#define I2C_SDA_PIN HW_I2C_SDA_PIN
#define I2C_CLOCK_HZ HW_I2C_CLOCK_HZ

// ===== UART / comms =====
#define COMM_BAUD 115200

// ===== NRF24 (SPI) =====
#define NRF_CSN_PIN HW_NRF_CSN_PIN
#define NRF_CE_PIN HW_NRF_CE_PIN
#define NRF_SCK_PIN HW_NRF_SCK_PIN
#define NRF_MOSI_PIN HW_NRF_MOSI_PIN
#define NRF_MISO_PIN HW_NRF_MISO_PIN
#define NRF_CHANNEL 76
#define NRF_PA_LEVEL 0 // RF24_PA_MIN (range: 0=MIN .. 3=MAX)

// ===== RGB LED =====
#define LED_RGB_PIN HW_LED_RGB_PIN

// ===== ADC model =====
#define ADC_BITS HW_ADC_BITS
#define ADC_MAX HW_ADC_MAX
#define ADC_CENTER HW_ADC_CENTER

// ===== EEPROM =====
// Set to 1 to force writing defaults to EEPROM on boot (use once, then set back to 0).
#define EEPROM_FORCE_DEFAULTS_ON_BOOT 0
