#include <Arduino.h>
#include "controller/config.h"
#include "common/comm.h"
#include "controller/receiver.h"
#include "controller/leds.h"
#include "controller/photo_sensor.h"

// ==================== Debug ====================
#define BATTERY_DEBUG 1 // 1 = print debug to Serial (USB), 0 = off

// ==================== Timing ====================
static const uint32_t TX_TICK_MS = 20;     // 50 Hz TX
static const uint32_t LED_TICK_MS = 10;    // 50 Hz LED update
static const uint32_t RX_TIMEOUT_MS = 120; // failsafe: if no valid RX frame for this long

// ==================== Filtering ====================
static const uint8_t EMA_SHIFT = 2; // EMA: 1/8

// Snap ends to avoid faint glow near 0 due to noise/EMA tail
static const uint8_t BATTERY_SNAP_LOW = 1;   // <1  -> 0
static const uint8_t BATTERY_SNAP_HIGH = 99; // >99 -> 100

// Near-zero LED hysteresis
static const uint8_t LED_OFF_TH = 2; // <=2  -> force off
static const uint8_t LED_ON_TH = 4;  // >=4  -> enable again

// ==================== State ====================
static uint16_t batteryPctTarget = 0; // filtered target (0..100)
static uint16_t batteryPctSmooth = 0; // EMA output (0..100)

static uint32_t lastTxMs = 0;
static uint32_t lastLedMs = 0;
static uint32_t lastRxOkMs = 0;

// Median-of-3 history (glitch killer)
static uint16_t s0 = 0, s1 = 0, s2 = 0;
static bool samplesInit = false;

static uint16_t clampAndSnap(uint16_t v)
{
    if (v > 100)
        v = 100;
    if (v < BATTERY_SNAP_LOW)
        v = 0;
    if (v > BATTERY_SNAP_HIGH)
        v = 100;
    return v;
}

static uint16_t median3(uint16_t a, uint16_t b, uint16_t c)
{
    // Return median (middle) of three values
    if (a > b)
    {
        uint16_t t = a;
        a = b;
        b = t;
    }
    if (b > c)
    {
        uint16_t t = b;
        b = c;
        c = t;
    }
    if (a > b)
    {
        uint16_t t = a;
        a = b;
        b = t;
    }
    return b;
}

static void updateLed()
{
    uint32_t now = millis();
    if (now - lastLedMs < LED_TICK_MS)
        return;
    lastLedMs = now;

    // Failsafe: if RX is stale, show "no link" color (optional).
    // If you want ALWAYS blue/purple even on link loss, remove this block.
    if (now - lastRxOkMs > RX_TIMEOUT_MS)
    {
        batteryPctTarget = 0; // fallback to 0% => blue
    }

    // EMA smoothing (MUST be signed to avoid uint underflow)
    int32_t delta = (int32_t)batteryPctTarget - (int32_t)batteryPctSmooth; // can be negative
    batteryPctSmooth = (uint16_t)((int32_t)batteryPctSmooth + (delta >> EMA_SHIFT));

    // Clamp just in case (should already be 0..100)
    if (batteryPctSmooth > 100)
        batteryPctSmooth = 100;

    // Color mapping:
    // 0%   -> Blue    (R=0, G=0, B=255)
    // 100% -> Purple  (R=255, G=0, B=255)
    uint8_t t = (uint8_t)map(batteryPctSmooth, 0, 100, 0, 255);
    Color c{0, t, 255}; // R increases, B stays max, G stays 0

    ledsSet(LedSlot::Third, c, photoSensorLedBrightnessPct());
}

void receiverInit()
{
    batteryPctTarget = 0;
    batteryPctSmooth = 0;

    lastTxMs = 0;
    lastLedMs = 0;
    lastRxOkMs = millis();

    s0 = s1 = s2 = 0;
    samplesInit = false;
}

void receiverLoop(const CommFrame &txFrame)
{
    uint32_t now = millis();

    // ===== TX max 50 Hz + ACK telemetry =====
    CommFrame rx{};
    bool got = false;
    uint16_t lastRaw = batteryPctTarget;

    if (now - lastTxMs >= TX_TICK_MS)
    {
        lastTxMs = now;
        if (commSendFrame(txFrame, &rx))
        {
            uint16_t v = clampAndSnap(rx.battPct);
            lastRaw = v;
            got = true;
            lastRxOkMs = now; // we got valid ACK telemetry
        }
    }

    // Apply median-of-3 glitch filter
    if (got)
    {
        if (!samplesInit)
        {
            s0 = s1 = s2 = lastRaw;
            samplesInit = true;
        }
        else
        {
            s0 = s1;
            s1 = s2;
            s2 = lastRaw;
        }

        batteryPctTarget = median3(s0, s1, s2);
    }

    // Update LED (no ledsShow() here)
    updateLed();

#if BATTERY_DEBUG
    // Print comm stats + values at low rate
    static uint32_t dbgTick = 0;
    if (now - dbgTick >= 500)
    {
        dbgTick = now;
        Serial.print("[RX BATT] rawPct=");
        Serial.print((unsigned)lastRaw);
        Serial.print(" target=");
        Serial.print((unsigned)batteryPctTarget);
        Serial.print(" smooth=");
        Serial.print((unsigned)batteryPctSmooth);
        Serial.println();
    }
#endif
}

uint16_t receiverGetBatteryPct()
{
    return batteryPctTarget;
}
