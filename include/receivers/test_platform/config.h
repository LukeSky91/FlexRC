#pragma once
#include <stdint.h>

#define ROLE_RECEIVER 1

// NRF24L01 pin mapping (Arduino Nano)
#define NRF24_CSN_PIN 8
#define NRF24_CE_PIN 7
#define NRF24_SCK_PIN 13
#define NRF24_MOSI_PIN 11
#define NRF24_MISO_PIN 12
#define NRF_CHANNEL 76
#define NRF_PA_LEVEL 0 // RF24_PA_MIN (range: 0=MIN .. 3=MAX)
static const uint8_t NRF_ADDR[5] = {'R', 'C', '0', '0', '1'};
#define NRF_ENABLED 1

#define AUX_PIN A1


#define SERIAL_ENABLED 1
#define SERIAL_BAUD 115200
