#pragma once
#include <Arduino.h>

/*
 * Returns true once every given interval (in milliseconds).
 *
 * Typical usage:
 *   static uint32_t lastTick = 0;
 *   if (everyMs(100, lastTick)) {
 *       // code executed every 100 ms
 *   }
 *
 * lastTick must be preserved between calls.
 */
bool everyMs(uint32_t interval, uint32_t &lastTick);
