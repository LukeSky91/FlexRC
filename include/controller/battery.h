#pragma once

#include <stdint.h>

struct BatteryReading
{
    uint16_t millivolts;
    uint8_t percent;
    bool valid;
};

void batteryInit();
void batteryTick();
BatteryReading batteryGetReading();
