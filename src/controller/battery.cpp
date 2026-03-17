 #include <Arduino.h>
#include "controller/battery.h"
#include "controller/config.h"

static BatteryReading gBattery{0, 0, false};
static uint32_t lastReadMs = 0;

static uint8_t batteryPctFromMv(uint32_t mv)
{
    if (mv <= BATTERY_CELL_EMPTY_MV)
        return 0;
    if (mv >= BATTERY_CELL_FULL_MV)
        return 100;

    const uint32_t span = (uint32_t)BATTERY_CELL_FULL_MV - (uint32_t)BATTERY_CELL_EMPTY_MV;
    return (uint8_t)(((mv - BATTERY_CELL_EMPTY_MV) * 100UL + (span / 2UL)) / span);
}

static uint32_t readBatteryMillivolts()
{
#if defined(ARDUINO_ARCH_ESP32)
    uint32_t adcMvSum = 0;
    for (uint8_t i = 0; i < BATTERY_AVG_SAMPLES; ++i)
    {
        adcMvSum += (uint32_t)analogReadMilliVolts(BATTERY_PIN);
    }

    const uint32_t adcMv = adcMvSum / BATTERY_AVG_SAMPLES;
#else
    uint32_t rawSum = 0;
    for (uint8_t i = 0; i < BATTERY_AVG_SAMPLES; ++i)
    {
        rawSum += (uint32_t)analogRead(BATTERY_PIN);
    }

    const uint32_t raw = rawSum / BATTERY_AVG_SAMPLES;
    const uint32_t adcMv = (raw * 3300UL + (ADC_MAX / 2UL)) / ADC_MAX;
#endif

    return (adcMv * (BATTERY_DIVIDER_R_TOP_OHM + BATTERY_DIVIDER_R_BOTTOM_OHM) + (BATTERY_DIVIDER_R_BOTTOM_OHM / 2UL)) /
           BATTERY_DIVIDER_R_BOTTOM_OHM;
}

void batteryInit()
{
    pinMode(BATTERY_PIN, INPUT);
#if defined(ARDUINO_ARCH_ESP32)
    analogReadResolution(ADC_BITS);
#endif
    lastReadMs = 0;
    gBattery = {0, 0, false};
}

void batteryTick()
{
    const uint32_t now = millis();
    if (now - lastReadMs < BATTERY_READ_INTERVAL_MS)
        return;

    lastReadMs = now;

    const uint32_t millivolts = readBatteryMillivolts();
    gBattery.millivolts = (millivolts > 0xFFFFUL) ? 0xFFFFU : (uint16_t)millivolts;
    gBattery.percent = batteryPctFromMv(gBattery.millivolts);
    gBattery.valid = true;
}

BatteryReading batteryGetReading()
{
    return gBattery;
}
