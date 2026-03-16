#pragma once

#include <stdint.h>

enum class PhotoBrightnessMode : uint8_t
{
    Sensor = 0,
    Fixed = 1
};

enum class PhotoFilterLevel : uint8_t
{
    Off = 0,
    Low,
    Med,
    High
};

enum class PhotoHysteresisLevel : uint8_t
{
    Off = 0,
    Low,
    Med,
    High
};

struct PhotoSensorConfig
{
    int minRaw;
    int maxRaw;
    uint8_t minLedPct;
    uint8_t maxLedPct;
    PhotoBrightnessMode mode;
    uint8_t fixedPct;
    PhotoFilterLevel filter;
    PhotoHysteresisLevel hysteresis;
};

void photoSensorInit();
int photoSensorReadRaw();
uint8_t photoSensorReadPct();
uint8_t photoSensorReadMappedPct();
uint8_t photoSensorLedBrightnessPct();

PhotoSensorConfig photoSensorGetConfig();
void photoSensorSetConfig(const PhotoSensorConfig &cfg);
void photoSensorSaveConfig();
