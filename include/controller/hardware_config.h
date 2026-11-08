#pragma once

// ESP32-S3-Pico hardware mapping — PCB rev V1.
// Source of truth: PCB/FlexRC/FlexRC.kicad_pcb (net names on pads of A1 = ESP32-S3-Pico).
// Breadboard mapping is no longer supported (last version: git tag `breadboard-working`).

// ===== ADC / analog inputs =====
#define HW_ADC_BITS 12
#define HW_ADC_MAX 4095
#define HW_ADC_CENTER 2048

// Gimbal L (JOY_L_1)
#define HW_JOY_L_PIN_Y 1   // net CH0 / ADC1_CH0 / GP1  <- JOY_L_1 Y2 (V_signal)
#define HW_JOY_L_PIN_X 2   // net CH1 / ADC1_CH1 / GP2  <- JOY_L_1 X2 (H_signal)
#define HW_JOY_L_PIN_BTN 4 // net CH3 / ADC1_CH3 / GP4  <- JOY_L_1 SW via R29 1k, pull-up R27 10k

// Gimbal R (JOY_R_1)
#define HW_JOY_R_PIN_Y 5   // net CH4 / ADC1_CH4 / GP5  <- JOY_R_1 Y2 (V_signal)
#define HW_JOY_R_PIN_X 6   // net CH5 / ADC1_CH5 / GP6  <- JOY_R_1 X2 (H_signal)
#define HW_JOY_R_PIN_BTN 7 // net CH6 / ADC1_CH6 / GP7  <- JOY_R_1 SW via R30 1k, pull-up R28 10k

#define HW_PHOTO_PIN 8     // net CH7 / ADC1_CH7 / GP8  <- R9 LDR / R10 3k3 / R11 10k, via R12 1k
#define HW_BATTERY_PIN 9   // net CH8 / ADC1_CH8 / GP9  <- divider R1 51k / (R2+R3) 20k, via R4 1k

// ===== Digital buttons (active LOW, 10k pull-up + 1k series) =====
#define HW_BTN_DOWN_PIN 11   // GP11 <- SW_D1 (R17 / R24)
#define HW_BTN_CENTER_PIN 12 // GP12 <- SW_C1 (R15 / R22)
#define HW_BTN_RIGHT_PIN 13  // GP13 <- SW_R1 (R16 / R23)
#define HW_BTN_LEFT_PIN 14   // GP14 <- SW_L1 (R14 / R21)
#define HW_BTN_UP_PIN 15     // GP15 <- SW_U1 (R13 / R20)
#define HW_BTN_F1_PIN 42     // GP42 <- SW_F1 (R19 / R26)
#define HW_BTN_F2_PIN 41     // GP41 <- SW_F2 (R18 / R25)

// ===== I2C (OLED DS1) =====
#define HW_I2C_SCL_PIN 17 // GP17 -> R7 33R -> DS1 SCL, pull-up R5 5k1
#define HW_I2C_SDA_PIN 18 // GP18 -> R8 33R -> DS1 SDA, pull-up R6 5k1
#define HW_I2C_CLOCK_HZ 400000

// ===== SPI / nRF24 (TX1) =====
#define HW_NRF_CE_PIN 34   // GP34 -> TX1 pad 3 CE
#define HW_NRF_SCK_PIN 35  // GP35 -> TX1 pad 5 SCK
#define HW_NRF_MISO_PIN 36 // GP36 <- TX1 pad 7 MISO
#define HW_NRF_MOSI_PIN 37 // GP37 -> TX1 pad 6 MOSI
#define HW_NRF_CSN_PIN 38  // GP38 -> TX1 pad 4 CSN

// ===== RGB LED =====
#define HW_LED_RGB_PIN 39 // GP39 -> U3 74AHCT1G125 -> R31 330R -> WS2812B D1..D5
