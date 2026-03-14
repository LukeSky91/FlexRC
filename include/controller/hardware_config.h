#pragma once

// ESP32-S3-Pico hardware mapping

// ===== ADC / analog inputs =====
#define HW_ADC_BITS 12
#define HW_ADC_MAX 4095
#define HW_ADC_CENTER 2048

#define HW_JOY_R_PIN_Y 1   // ADC1_CH0 / GP1
#define HW_JOY_R_PIN_X 2   // ADC1_CH1 / GP2
#define HW_JOY_L_PIN_Y 4   // ADC1_CH3 / GP4
#define HW_JOY_L_PIN_X 5   // ADC1_CH4 / GP5
#define HW_JOY_R_PIN_BTN 6 // ADC1_CH5 / GP6
#define HW_JOY_L_PIN_BTN 7 // ADC1_CH6 / GP7

#define HW_PHOTO_PIN 8     // ADC1_CH7 / GP8
#define HW_BATTERY_PIN 9   // ADC1_CH8 / GP9

// ===== Digital buttons =====
#define HW_BTN_UP_PIN 11
#define HW_BTN_LEFT_PIN 12
#define HW_BTN_CENTER_PIN 13
#define HW_BTN_RIGHT_PIN 14
#define HW_BTN_DOWN_PIN 15
#define HW_BTN_F1_PIN 42
#define HW_BTN_F2_PIN 41

// ===== I2C =====
#define HW_I2C_SCL_PIN 17
#define HW_I2C_SDA_PIN 18
#define HW_I2C_CLOCK_HZ 400000

// ===== SPI / nRF24 =====
#define HW_NRF_SCK_PIN 34
#define HW_NRF_MISO_PIN 35
#define HW_NRF_MOSI_PIN 36
#define HW_NRF_CSN_PIN 37
#define HW_NRF_CE_PIN 38

// ===== RGB LED =====
#define HW_LED_RGB_PIN 39
