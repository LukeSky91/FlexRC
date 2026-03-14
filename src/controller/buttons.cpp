#include <Arduino.h>
#include "controller/buttons.h"
#include "controller/config.h"
#include "controller/storage.h"

static const unsigned long debounceMs = 30;
static const bool BUTTONS_MONITOR = (PERF_DEBUG != 0);
static const uint8_t KEY_SLOT_COUNT = (uint8_t)Key::F2 + 1;

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
static const char *STORAGE_KEY_BUTTONS = "btn_thresh";

static uint16_t crcKeys(const KeyThrData &d)
{
    return (uint16_t)(d.magic ^ d.thDown ^ d.thUp ^ d.thRight ^ d.thCenter ^ d.thLeft ^ 0xA55A);
}

static inline uint8_t idx(Key k) { return (uint8_t)k; }

struct Engine
{
    Key lastReading = Key::None;
    Key stable = Key::None;
    unsigned long lastChange = 0;
    unsigned long pressStart = 0;
    bool releasedPending[KEY_SLOT_COUNT] = {};
    uint32_t releaseDur[KEY_SLOT_COUNT] = {};
    bool shortPending[KEY_SLOT_COUNT] = {};
    bool longFired[KEY_SLOT_COUNT] = {};
    unsigned long lastRepeatAt[KEY_SLOT_COUNT] = {};
};

static Engine eng;
static uint32_t lastReleaseDurationMs = 0;
static Key lastReleaseKey = Key::None;

static inline bool isPhysicalPressed(uint8_t pin)
{
    return digitalRead(pin) == LOW;
}

static inline void clampThresholds()
{
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

static const char *keyName(Key k)
{
    switch (k)
    {
    case Key::Left: return "LEFT";
    case Key::Right: return "RIGHT";
    case Key::Up: return "UP";
    case Key::Down: return "DOWN";
    case Key::Center: return "CENTER";
    case Key::F1: return "F1";
    case Key::F2: return "F2";
    default: return "NONE";
    }
}

static void resetPerPressState(Key k)
{
    const uint8_t i = idx(k);
    if (i >= KEY_SLOT_COUNT)
        return;
    eng.longFired[i] = false;
    eng.lastRepeatAt[i] = 0;
}

static Key selectFirstPressed(Key preferred)
{
    switch (preferred)
    {
    case Key::Left:
        if (isPhysicalPressed(BUTTON_LEFT_PIN))
            return preferred;
        break;
    case Key::Right:
        if (isPhysicalPressed(BUTTON_RIGHT_PIN))
            return preferred;
        break;
    case Key::Up:
        if (isPhysicalPressed(BUTTON_UP_PIN))
            return preferred;
        break;
    case Key::Down:
        if (isPhysicalPressed(BUTTON_DOWN_PIN))
            return preferred;
        break;
    case Key::Center:
        if (isPhysicalPressed(BUTTON_CENTER_PIN))
            return preferred;
        break;
    case Key::F1:
        if (isPhysicalPressed(BUTTON_F1_PIN))
            return preferred;
        break;
    case Key::F2:
        if (isPhysicalPressed(BUTTON_F2_PIN))
            return preferred;
        break;
    default:
        break;
    }

    if (isPhysicalPressed(BUTTON_UP_PIN))
        return Key::Up;
    if (isPhysicalPressed(BUTTON_DOWN_PIN))
        return Key::Down;
    if (isPhysicalPressed(BUTTON_LEFT_PIN))
        return Key::Left;
    if (isPhysicalPressed(BUTTON_RIGHT_PIN))
        return Key::Right;
    if (isPhysicalPressed(BUTTON_CENTER_PIN))
        return Key::Center;
    if (isPhysicalPressed(BUTTON_F1_PIN))
        return Key::F1;
    if (isPhysicalPressed(BUTTON_F2_PIN))
        return Key::F2;
    return Key::None;
}

static void buttonsUpdate()
{
    const Key reading = selectFirstPressed(eng.stable);

    if (reading != eng.lastReading)
    {
        eng.lastChange = millis();
        eng.lastReading = reading;
    }

    if ((millis() - eng.lastChange) <= debounceMs)
        return;

    if (reading == eng.stable)
        return;

    const Key prev = eng.stable;
    eng.stable = reading;

    if (prev == Key::None && eng.stable != Key::None)
    {
        eng.pressStart = millis();
        resetPerPressState(eng.stable);

        if (BUTTONS_MONITOR)
        {
            Serial.print("[BTN] PRESSED  ");
            Serial.println(keyName(eng.stable));
        }
        return;
    }

    if (prev != Key::None)
    {
        const uint8_t ip = idx(prev);
        uint32_t dur = 0;
        if (eng.pressStart != 0)
            dur = (uint32_t)(millis() - eng.pressStart);

        if (ip < KEY_SLOT_COUNT)
        {
            eng.releaseDur[ip] = dur;
            eng.releasedPending[ip] = true;
            if (!eng.longFired[ip])
                eng.shortPending[ip] = true;
        }

        lastReleaseDurationMs = dur;
        lastReleaseKey = prev;

        if (BUTTONS_MONITOR)
        {
            Serial.print("[BTN] RELEASED ");
            Serial.print(keyName(prev));
            Serial.print("  dur=");
            Serial.print(dur);
            Serial.println(" ms");
        }
    }

    if (eng.stable != Key::None)
    {
        eng.pressStart = millis();
        resetPerPressState(eng.stable);

        if (BUTTONS_MONITOR)
        {
            Serial.print("[BTN] PRESSED  ");
            Serial.println(keyName(eng.stable));
        }
    }
    else
    {
        eng.pressStart = 0;
    }
}

void buttonsInit()
{
    pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
    pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
    pinMode(BUTTON_LEFT_PIN, INPUT_PULLUP);
    pinMode(BUTTON_RIGHT_PIN, INPUT_PULLUP);
    pinMode(BUTTON_CENTER_PIN, INPUT_PULLUP);
    pinMode(BUTTON_F1_PIN, INPUT_PULLUP);
    pinMode(BUTTON_F2_PIN, INPUT_PULLUP);

    eng = Engine{};
    clampThresholds();

    KeyThrData d{};
    if (storageReadBlob(STORAGE_KEY_BUTTONS, &d, sizeof(d)) &&
        d.magic == KEYS_MAGIC && d.crc == crcKeys(d))
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
    const uint8_t i = idx(k);
    if (i >= KEY_SLOT_COUNT)
        return false;

    if (!eng.releasedPending[i])
        return false;

    if (durationMs)
        *durationMs = eng.releaseDur[i];

    if (consume)
    {
        eng.releasedPending[i] = false;
        eng.shortPending[i] = false;
    }
    return true;
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
    buttonsUpdate();
    switch (eng.stable)
    {
    case Key::Down: return (uint16_t)TH_DOWN;
    case Key::Up: return (uint16_t)TH_UP;
    case Key::Right: return (uint16_t)TH_RIGHT;
    case Key::Center: return (uint16_t)TH_CENTER;
    case Key::Left: return (uint16_t)TH_LEFT;
    default: return 0;
    }
}

int buttonsGetThreshold(Key k)
{
    switch (k)
    {
    case Key::Down: return TH_DOWN;
    case Key::Up: return TH_UP;
    case Key::Right: return TH_RIGHT;
    case Key::Center: return TH_CENTER;
    case Key::Left: return TH_LEFT;
    default: return 0;
    }
}

void buttonsSetThreshold(Key k, int value)
{
    switch (k)
    {
    case Key::Down: TH_DOWN = value; break;
    case Key::Up: TH_UP = value; break;
    case Key::Right: TH_RIGHT = value; break;
    case Key::Center: TH_CENTER = value; break;
    case Key::Left: TH_LEFT = value; break;
    default: break;
    }
    clampThresholds();
}

void buttonsAdjustThreshold(Key k, int delta)
{
    buttonsSetThreshold(k, buttonsGetThreshold(k) + delta);
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
    storageWriteBlob(STORAGE_KEY_BUTTONS, &d, sizeof(d));
}

void buttonsConsumeAll()
{
    const Key current = eng.stable;
    eng = Engine{};
    eng.stable = current;
    eng.lastReading = current;
    lastReleaseDurationMs = 0;
    lastReleaseKey = Key::None;
}

bool keyShortClick(Key k, uint32_t thresholdMs, bool consume)
{
    buttonsUpdate();
    const uint8_t i = idx(k);
    if (i >= KEY_SLOT_COUNT || !eng.shortPending[i])
        return false;

    const bool isShort = (eng.releaseDur[i] < thresholdMs);
    if (consume)
    {
        eng.shortPending[i] = false;
        eng.releasedPending[i] = false;
    }
    return isShort;
}

bool keyLongPress(Key k,
                  bool repeat,
                  uint32_t repeatMs,
                  uint32_t thresholdMs,
                  bool consume)
{
    buttonsUpdate();

    if (k == Key::None || eng.stable != k || eng.pressStart == 0)
        return false;

    const uint8_t i = idx(k);
    if (i >= KEY_SLOT_COUNT)
        return false;

    const uint32_t held = (uint32_t)(millis() - eng.pressStart);
    if (held < thresholdMs)
        return false;

    if (!eng.longFired[i])
    {
        if (consume)
            eng.longFired[i] = true;
        eng.lastRepeatAt[i] = millis();
        return true;
    }

    if (repeat)
    {
        const unsigned long now = millis();
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
