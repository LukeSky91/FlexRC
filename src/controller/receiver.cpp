#include <Arduino.h>
#include "controller/config.h"
#include "common/comm.h"
#include "controller/receiver.h"
#include "controller/leds.h"
#include "controller/photo_sensor.h"

// ==================== Debug ====================
#define BATTERY_DEBUG 1 // 1 = print debug to Serial (USB), 0 = off
#define LINK_DEBUG 1    // 1 = print link state changes to Serial (USB), 0 = off

// ==================== Timing ====================
static const uint32_t TX_TICK_MS = 20;     // 50 Hz TX
static const uint32_t LED_TICK_MS = 10;    // 50 Hz LED update
static const uint32_t RX_TIMEOUT_MS = 120; // failsafe: if no valid RX frame for this long
static const uint32_t LINK_LED_BLINK_MS = 250;

// ==================== Filtering ====================
static const uint8_t EMA_SHIFT = 2; // EMA: 1/8

// Snap ends to avoid faint glow near 0 due to noise/EMA tail
static const uint8_t BATTERY_SNAP_LOW = 1;   // <1  -> 0
static const uint8_t BATTERY_SNAP_HIGH = 99; // >99 -> 100

// ==================== State ====================
static uint16_t batteryPctTarget = 0; // filtered target (0..100)
static uint16_t batteryPctSmooth = 0; // EMA output (0..100)

static bool gRadioReady = false;
static bool gLinkEnabled = false;
static ReceiverLinkState gLinkState = ReceiverLinkState::Idle;

static uint32_t lastTxMs = 0;
static uint32_t lastLedMs = 0;
static uint32_t lastRxOkMs = 0;

// Median-of-3 history (glitch killer)
static uint16_t s0 = 0, s1 = 0, s2 = 0;
static bool samplesInit = false;

static const char *linkStateName(ReceiverLinkState state)
{
    switch (state)
    {
    case ReceiverLinkState::Idle:
        return "IDLE";
    case ReceiverLinkState::Connecting:
        return "CONNECTING";
    case ReceiverLinkState::Connected:
        return "CONNECTED";
    case ReceiverLinkState::Lost:
        return "LOST";
    case ReceiverLinkState::RadioError:
        return "RADIO_ERROR";
    }
    return "UNKNOWN";
}

static void setLinkState(ReceiverLinkState state)
{
    if (gLinkState == state)
        return;

    gLinkState = state;

#if LINK_DEBUG
    Serial.print("[LINK] ");
    Serial.println(linkStateName(state));
#endif
}

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

static void updateLinkLed()
{
    const uint8_t brightnessPct = photoSensorLedBrightnessPct();
    const bool blinkOn = ((millis() / LINK_LED_BLINK_MS) % 2U) == 0U;
    Color c = OFF;

    switch (gLinkState)
    {
    case ReceiverLinkState::Idle:
        c = YELLOW;
        break;
    case ReceiverLinkState::Connecting:
        c = blinkOn ? BLUE : OFF;
        break;
    case ReceiverLinkState::Connected:
        c = GREEN;
        break;
    case ReceiverLinkState::Lost:
        c = blinkOn ? RED : OFF;
        break;
    case ReceiverLinkState::RadioError:
        c = RED;
        break;
    }

    ledsSet(LedSlot::First, c, brightnessPct);
}

static void updateLed()
{
    uint32_t now = millis();
    if (now - lastLedMs < LED_TICK_MS)
        return;
    lastLedMs = now;

    if (!gLinkEnabled || now - lastRxOkMs > RX_TIMEOUT_MS)
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

    updateLinkLed();
    ledsSet(LedSlot::Third, c, photoSensorLedBrightnessPct());
}

void receiverInit(bool radioReady)
{
    gRadioReady = radioReady;
    gLinkEnabled = false;

    batteryPctTarget = 0;
    batteryPctSmooth = 0;

    lastTxMs = 0;
    lastLedMs = 0;
    lastRxOkMs = millis();

    s0 = s1 = s2 = 0;
    samplesInit = false;

    setLinkState(gRadioReady ? ReceiverLinkState::Idle : ReceiverLinkState::RadioError);
}

void receiverSetLinkEnabled(bool enabled)
{
    if (!gRadioReady)
    {
        gLinkEnabled = false;
        setLinkState(ReceiverLinkState::RadioError);
        return;
    }

    gLinkEnabled = enabled;
    if (!gLinkEnabled)
    {
        setLinkState(ReceiverLinkState::Idle);
        return;
    }

    lastRxOkMs = millis();
    setLinkState(ReceiverLinkState::Connecting);
}

bool receiverIsLinkEnabled()
{
    return gLinkEnabled;
}

ReceiverLinkState receiverGetLinkState()
{
    return gLinkState;
}

const char *receiverGetLinkStateShortName()
{
    switch (gLinkState)
    {
    case ReceiverLinkState::Idle:
        return "IDLE";
    case ReceiverLinkState::Connecting:
        return "CONN";
    case ReceiverLinkState::Connected:
        return "OK";
    case ReceiverLinkState::Lost:
        return "LOST";
    case ReceiverLinkState::RadioError:
        return "ERR";
    }
    return "ERR";
}

void receiverLoop(const CommFrame &txFrame)
{
    uint32_t now = millis();

    if (!gRadioReady)
    {
        setLinkState(ReceiverLinkState::RadioError);
        updateLed();
        return;
    }

    if (!gLinkEnabled)
    {
        setLinkState(ReceiverLinkState::Idle);
        updateLed();
        return;
    }

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
            setLinkState(ReceiverLinkState::Connected);
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

    if (!got && gLinkState == ReceiverLinkState::Connected && now - lastRxOkMs > RX_TIMEOUT_MS)
    {
        setLinkState(ReceiverLinkState::Lost);
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
