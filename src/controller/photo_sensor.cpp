#include <Arduino.h>

#include "controller/photo_sensor.h"
#include "controller/config.h"

void photoSensorInit()
{
    pinMode(PHOTO_PIN, INPUT);
}

int photoSensorReadRaw()
{
    int raw = analogRead(PHOTO_PIN);
    if (raw < 0)
        raw = 0;
    if (raw > ADC_MAX)
        raw = ADC_MAX;
    return raw;
}

uint8_t photoSensorReadPct()
{
    const int raw = photoSensorReadRaw();
    long pct = (long)raw * 100 / ADC_MAX;
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    return (uint8_t)pct;
}
