#include <common/time_utils.h>

/*
 * Time-based periodic trigger helper.
 *
 * Uses millis() and unsigned arithmetic, so it is safe
 * across millis() overflow.
 */
bool everyMs(uint32_t interval, uint32_t &lastTick)
{
    uint32_t now = millis();
    if (now - lastTick >= interval)
    {
        lastTick = now;
        return true;
    }
    return false;
}
