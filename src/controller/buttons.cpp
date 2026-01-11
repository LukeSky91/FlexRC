#include <Arduino.h>
#include <EEPROM.h>
#include "controller/joysticks.h"
#include "controller/buttons.h"
#include "controller/config.h"

#ifdef BUTTONS_KEY_PIN
static const uint8_t KEY_PIN = BUTTONS_KEY_PIN;
#else
static const uint8_t KEY_PIN = A7;
#endif

static const uint8_t ADC_SAMPLES = 8;
static const unsigned long debounceMs = 30;

/*
 * Analog keyboard on A7 (resistor ladder).
 *
 * Legacy naming:
 *  - LEFT  == old BTN1
 *  - RIGHT == old BTN2
 *
 * Your measured AVG levels:
 * NONE≈8
 * DOWN≈603
 * UP≈693
 * RIGHT≈763
 * CENTER≈847
 * LEFT≈922
 *
 * Midpoints:
 */
// Per-key threshold model (pick the highest crossed threshold)
static int TH_DOWN = TH_DOWN_DEFAULT;
static int TH_UP = TH_UP_DEFAULT;
static int TH_RIGHT = TH_RIGHT_DEFAULT;
static int TH_CENTER = TH_CENTER_DEFAULT;
static int TH_LEFT = TH_LEFT_DEFAULT;

struct KeyThrData
{
    uint16_t magic;
    uint16_t thDown;
    uint16_t thUp;
    uint16_t thRight;
    uint16_t thCenter;
    uint16_t thLeft;
    uint16_t crc;
};

static const uint16_t KEYS_MAGIC = 0x4B59; // 'KY'

static uint16_t crcKeys(const KeyThrData &d)
{
    return (uint16_t)(d.magic ^ d.thDown ^ d.thUp ^ d.thRight ^ d.thCenter ^ d.thLeft ^ 0xA55A);
}

// ===== DIAGNOSTIC MONITOR =====
// true  -> prints only events: PRESSED / RELEASED + time
// false -> silent
static const bool BUTTONS_MONITOR = (PERF_DEBUG != 0);

static inline uint8_t idx(Key k) { return (uint8_t)k; }
static uint32_t lastReleaseDurationMs = 0;
static Key lastReleaseKey = Key::None;
static inline void clampThresholds()
{
    // keep range 0..1023; no ordering required (we pick the highest crossed)
    auto clamp = [](int &v)
    {
        if (v < 0)
            v = 0;
        if (v > 1023)
            v = 1023;
    };
    clamp(TH_DOWN);
    clamp(TH_UP);
    clamp(TH_RIGHT);
    clamp(TH_CENTER);
    clamp(TH_LEFT);
}

static int readAdcAvg()
{
    long sum = 0;
    for (uint8_t i = 0; i < ADC_SAMPLES; i++)
        sum += analogRead(KEY_PIN);
    return (int)(sum / ADC_SAMPLES);
}

static Key decodeKeyFromAdc(int adc)
{
    Key best = Key::None;
    int bestThr = -1;

    struct Entry
    {
        Key k;
        int thr;
    };
    const Entry entries[] = {
        {Key::Down, TH_DOWN},
        {Key::Up, TH_UP},
        {Key::Right, TH_RIGHT},
        {Key::Center, TH_CENTER},
        {Key::Left, TH_LEFT}};

    for (const auto &e : entries)
    {
        if (adc >= e.thr && e.thr >= 0 && e.thr <= 1023 && e.thr >= bestThr)
        {
            bestThr = e.thr;
            best = e.k;
        }
    }
    return best;
}

static const char *keyName(Key k)
{
    switch (k)
    {
    case Key::Left:
        return "LEFT"; // BTN1
    case Key::Right:
        return "RIGHT"; // BTN2
    case Key::Up:
        return "UP";
    case Key::Down:
        return "DOWN";
    case Key::Center:
        return "CENTER";
    default:
        return "NONE";
    }
}

struct Engine
{
    Key lastReading = Key::None;
    Key stable = Key::None;
    unsigned long lastChange = 0;

    // timing
    unsigned long pressStart = 0;

    // release event per key
    bool releasedPending[6] = {false, false, false, false, false, false};
    uint32_t releaseDur[6] = {0, 0, 0, 0, 0, 0};

    // short click per key (computed at release time)
    bool shortPending[6] = {false, false, false, false, false, false};

    // long press
    bool longFired[6] = {false, false, false, false, false, false};
    unsigned long lastRepeatAt[6] = {0, 0, 0, 0, 0, 0};
};

static Engine eng;

static void resetPerPressState(Key k)
{
    uint8_t i = idx(k);
    if (i >= 6)
        return;
    eng.longFired[i] = false;
    eng.lastRepeatAt[i] = 0;
}

static void buttonsUpdate()
{
    int adc = readAdcAvg();
    Key reading = decodeKeyFromAdc(adc);

    // require going below the lowest threshold before accepting another key (anti-chatter)
    int minThr = TH_DOWN;
    if (TH_UP < minThr)
        minThr = TH_UP;
    if (TH_RIGHT < minThr)
        minThr = TH_RIGHT;
    if (TH_CENTER < minThr)
        minThr = TH_CENTER;
    if (TH_LEFT < minThr)
        minThr = TH_LEFT;
    if (eng.stable != Key::None && reading != Key::None && reading != eng.stable && adc >= minThr)
    {
        reading = eng.stable;
    }

    if (reading != eng.lastReading)
    {
        eng.lastChange = millis();
        eng.lastReading = reading;
    }

    if ((millis() - eng.lastChange) > debounceMs)
    {
        if (reading != eng.stable)
        {
            Key prev = eng.stable;
            eng.stable = reading;

            // None -> k : pressed
            if (prev == Key::None && eng.stable != Key::None)
            {
                eng.pressStart = millis();
                resetPerPressState(eng.stable);

                if (BUTTONS_MONITOR)
                {
                    Serial.print("[BTN] PRESSED  ");
                    Serial.println(keyName(eng.stable));
                }
            }
            // k -> None : released
            else if (prev != Key::None && eng.stable == Key::None)
            {
                uint8_t ip = idx(prev);

                uint32_t dur = 0;
                if (eng.pressStart != 0)
                    dur = (uint32_t)(millis() - eng.pressStart);

                eng.releaseDur[ip] = dur;
                eng.releasedPending[ip] = true;
                lastReleaseDurationMs = dur;
                lastReleaseKey = prev;

                // short click is set ONLY if long did not fire
                if (!eng.longFired[ip])
                {
                    eng.shortPending[ip] = true;
                }

                if (BUTTONS_MONITOR)
                {
                    Serial.print("[BTN] RELEASED ");
                    Serial.print(keyName(prev));
                    Serial.print("  dur=");
                    Serial.print(dur);
                    Serial.println(" ms");
                }

                eng.pressStart = 0;
            }
            // k1 -> k2 (rare, but handled)
            else if (prev != Key::None && eng.stable != Key::None)
            {
                uint8_t ip = idx(prev);

                uint32_t dur = 0;
                if (eng.pressStart != 0)
                    dur = (uint32_t)(millis() - eng.pressStart);

                eng.releaseDur[ip] = dur;
                eng.releasedPending[ip] = true;
                lastReleaseDurationMs = dur;
                lastReleaseKey = prev;
                if (!eng.longFired[ip])
                    eng.shortPending[ip] = true;

                if (BUTTONS_MONITOR)
                {
                    Serial.print("[BTN] RELEASED ");
                    Serial.print(keyName(prev));
                    Serial.print("  dur=");
                    Serial.print(dur);
                    Serial.println(" ms");
                    Serial.print("[BTN] PRESSED  ");
                    Serial.println(keyName(eng.stable));
                }

                eng.pressStart = millis();
                resetPerPressState(eng.stable);
            }
        }
    }
}

void buttonsInit()
{
    pinMode(KEY_PIN, INPUT);
    eng = Engine{};
    clampThresholds();

    // Load thresholds from EEPROM if present
    uint16_t base = joysticksEepromAddrAfterExpo();
    KeyThrData d{};
    EEPROM.get(base, d);
    if (d.magic == KEYS_MAGIC && d.crc == crcKeys(d))
    {
        TH_DOWN = d.thDown;
        TH_UP = d.thUp;
        TH_RIGHT = d.thRight;
        TH_CENTER = d.thCenter;
        TH_LEFT = d.thLeft;
        clampThresholds();
    }
}

Key buttonsCurrent()
{
    buttonsUpdate();
    return eng.stable;
}

bool keyDown(Key k)
{
    buttonsUpdate();
    return eng.stable == k;
}

bool keyReleased(Key k, uint32_t *durationMs, bool consume)
{
    buttonsUpdate();
    uint8_t i = idx(k);
    if (i >= 6)
        return false;

    if (eng.releasedPending[i])
    {
        if (durationMs)
            *durationMs = eng.releaseDur[i];

        if (consume)
        {
            eng.releasedPending[i] = false;
            eng.shortPending[i] = false;
        }
        return true;
    }
    return false;
}

uint32_t buttonsLastReleaseDuration()
{
    return lastReleaseDurationMs;
}

Key buttonsLastReleaseKey()
{
    return lastReleaseKey;
}

uint16_t buttonsReadRawAdc()
{
    int v = readAdcAvg();
    if (v < 0)
        v = 0;
    if (v > 1023)
        v = 1023;
    return (uint16_t)v;
}

int buttonsGetThreshold(Key k)
{
    switch (k)
    {
    case Key::Down:
        return TH_DOWN;
    case Key::Up:
        return TH_UP;
    case Key::Right:
        return TH_RIGHT;
    case Key::Center:
        return TH_CENTER;
    case Key::Left:
        return TH_LEFT;
    default:
        return 0;
    }
}

void buttonsSetThreshold(Key k, int value)
{
    switch (k)
    {
    case Key::Down:
        TH_DOWN = value;
        break;
    case Key::Up:
        TH_UP = value;
        break;
    case Key::Right:
        TH_RIGHT = value;
        break;
    case Key::Center:
        TH_CENTER = value;
        break;
    case Key::Left:
        TH_LEFT = value;
        break;
    default:
        break;
    }
    clampThresholds();
}

void buttonsAdjustThreshold(Key k, int delta)
{
    int v = buttonsGetThreshold(k);
    buttonsSetThreshold(k, v + delta);
}

void buttonsSaveThresholds()
{
    KeyThrData d{};
    d.magic = KEYS_MAGIC;
    d.thDown = TH_DOWN;
    d.thUp = TH_UP;
    d.thRight = TH_RIGHT;
    d.thCenter = TH_CENTER;
    d.thLeft = TH_LEFT;
    d.crc = crcKeys(d);

    uint16_t base = joysticksEepromAddrAfterExpo();
    EEPROM.put(base, d);
}

void buttonsConsumeAll()
{
    // reset engine state, keep thresholds
    Key current = eng.stable;
    eng = Engine{};
    eng.stable = current; // preserve currently held key
    lastReleaseDurationMs = 0;
    lastReleaseKey = Key::None;
}

bool keyShortClick(Key k, uint32_t thresholdMs, bool consume)
{
    buttonsUpdate();
    uint8_t i = idx(k);
    if (i >= 6)
        return false;

    if (eng.shortPending[i])
    {
        const bool isShort = (eng.releaseDur[i] < thresholdMs);

        if (consume)
        {
            eng.shortPending[i] = false;
            eng.releasedPending[i] = false;
        }

        return isShort;
    }
    return false;
}

bool keyLongPress(Key k,
                  bool repeat,
                  uint32_t repeatMs,
                  uint32_t thresholdMs,
                  bool consume)
{
    buttonsUpdate();

    if (k == Key::None)
        return false;
    if (eng.stable != k)
        return false;
    if (eng.pressStart == 0)
        return false;

    uint8_t i = idx(k);
    if (i >= 6)
        return false;

    uint32_t held = (uint32_t)(millis() - eng.pressStart);
    if (held < thresholdMs)
        return false;

    // first long fires exactly at threshold crossing
    if (!eng.longFired[i])
    {
        if (consume)
            eng.longFired[i] = true;
        eng.lastRepeatAt[i] = millis();
        return true;
    }

    // optional auto-repeat
    if (repeat)
    {
        unsigned long now = millis();
        if (now - eng.lastRepeatAt[i] >= repeatMs)
        {
            eng.lastRepeatAt[i] = now;
            return true;
        }
    }

    return false;
}

void buttonsTick()
{
    buttonsUpdate();
}
