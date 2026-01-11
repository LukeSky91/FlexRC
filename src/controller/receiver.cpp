#include <Arduino.h>
#include "controller/config.h"
#include "common/comm.h"
#include "controller/receiver.h"
#include "controller/leds.h"

// ==================== Debug ====================
#define AUX_DEBUG 1 // 1 = print debug to Serial (USB), 0 = off

// ==================== Timing ====================
static const uint32_t TX_TICK_MS = 20;     // 50 Hz TX
static const uint32_t LED_TICK_MS = 10;    // 50 Hz LED update
static const uint32_t RX_TIMEOUT_MS = 120; // failsafe: if no valid RX frame for this long

// ==================== Filtering ====================
static const uint8_t EMA_SHIFT = 2; // EMA: 1/8

// Snap ends to avoid faint glow near 0 due to noise/EMA tail
static const uint8_t AUX_SNAP_LOW = 1;   // <1  -> 0
static const uint8_t AUX_SNAP_HIGH = 99; // >99 -> 100

// Near-zero LED hysteresis
static const uint8_t LED_OFF_TH = 2; // <=2  -> force off
static const uint8_t LED_ON_TH = 4;  // >=4  -> enable again

// ==================== State ====================
static uint16_t auxTarget = 0; // filtered target (0..100)
static uint16_t auxSmooth = 0; // EMA output (0..100)

static uint32_t lastTxMs = 0;
static uint32_t lastLedMs = 0;
static uint32_t lastRxOkMs = 0;

// Median-of-3 history (glitch killer)
static uint16_t s0 = 0, s1 = 0, s2 = 0;
static bool samplesInit = false;

// LED global brightness (percent, after RGB computed)
static const uint8_t LED_BRIGHT_PCT = 15; // 10–20% nie razi

// Map 0..100% -> hue (blue..red). 0=>blue(160), 100=>red(0)
// Zmień kolejność jeśli chcesz red->blue: map(..., 0,100,0,160)
static inline uint16_t pctToHue(uint16_t pct)
{
    if (pct > 100)
        pct = 100;
    return (uint16_t)map(pct, 0, 100, 240, 0); // 240≈blue, 0=red
}

// Fast HSV->RGB (8-bit), H:0..360 (tu używamy 0..160)
static inline Color hsvToRgb(uint16_t h, uint8_t s, uint8_t v)
{
    uint8_t region = (h / 60) % 6;
    uint16_t f = (h % 60) * 255 / 60;
    uint8_t p = (uint8_t)((uint16_t)v * (255 - s) / 255);
    uint8_t q = (uint8_t)((uint16_t)v * (255 - (uint16_t)f * s / 255) / 255);
    uint8_t t = (uint8_t)((uint16_t)v * (255 - (uint16_t)(255 - f) * s / 255) / 255);

    uint8_t r = 0, g = 0, b = 0;
    switch (region)
    {
    case 0:
        r = v;
        g = t;
        b = p;
        break;
    case 1:
        r = q;
        g = v;
        b = p;
        break;
    case 2:
        r = p;
        g = v;
        b = t;
        break;
    case 3:
        r = p;
        g = q;
        b = v;
        break;
    case 4:
        r = t;
        g = p;
        b = v;
        break;
    default:
        r = v;
        g = p;
        b = q;
        break; // region 5
    }
    return Color{r, g, b};
}

static uint16_t clampAndSnap(uint16_t v)
{
    if (v > 100)
        v = 100;
    if (v < AUX_SNAP_LOW)
        v = 0;
    if (v > AUX_SNAP_HIGH)
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
        auxTarget = 0; // fallback to 0% => blue
    }

    // EMA smoothing (MUST be signed to avoid uint underflow)
    int32_t delta = (int32_t)auxTarget - (int32_t)auxSmooth; // can be negative
    auxSmooth = (uint16_t)((int32_t)auxSmooth + (delta >> EMA_SHIFT));

    // Clamp just in case (should already be 0..100)
    if (auxSmooth > 100)
        auxSmooth = 100;

    // Color mapping:
    // 0%   -> Blue    (R=0, G=0, B=255)
    // 100% -> Purple  (R=255, G=0, B=255)
    uint8_t t = (uint8_t)map(auxSmooth, 0, 100, 0, 255);
    Color c{0, t, 255}; // R increases, B stays max, G stays 0

    ledsSet(LedSlot::Third, c, LED_BRIGHT_PCT);
}

void receiverInit()
{
    auxTarget = 0;
    auxSmooth = 0;

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
    uint16_t lastRaw = auxTarget;

    if (now - lastTxMs >= TX_TICK_MS)
    {
        lastTxMs = now;
        if (commSendFrame(txFrame, &rx))
        {
            uint16_t v = clampAndSnap(rx.aux);
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

        auxTarget = median3(s0, s1, s2);
    }

    // Update LED (no ledsShow() here)
    updateLed();

#if AUX_DEBUG
    // Print comm stats + values at low rate
    static uint32_t dbgTick = 0;
    if (now - dbgTick >= 500)
    {
        dbgTick = now;
        Serial.print("[AUX] raw=");
        Serial.print((unsigned)lastRaw);
        Serial.print(" target=");
        Serial.print((unsigned)auxTarget);
        Serial.print(" smooth=");
        Serial.print((unsigned)auxSmooth);
        Serial.println();
    }
#endif
}

uint16_t receiverGetLastAux()
{
    return auxTarget; // filtered target (median-of-3)
}
