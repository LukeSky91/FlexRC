#pragma once
#include <stdint.h>

enum class LedSlot : uint8_t {
    First  = 0,  // 1ST (LED1)
    Second = 1,  // 2ND (LED2)
    Third  = 2   // 3RD (LED3)
};

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

constexpr Color RED     = {255, 0, 0};
constexpr Color GREEN   = {0, 255, 0};
constexpr Color BLUE    = {0, 0, 255};
constexpr Color YELLOW  = {255, 255, 0};
constexpr Color WHITE   = {255, 255, 255};
constexpr Color OFF     = {0, 0, 0};

void ledsInit();
void ledsSet(LedSlot slot, Color c, uint8_t brightnessPct = 50);
void ledsShow();
void ledsAllOff();
