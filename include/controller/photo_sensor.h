#pragma once

#include <stdint.h>

void photoSensorInit();
int photoSensorReadRaw();
uint8_t photoSensorReadPct();
