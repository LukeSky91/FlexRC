#include <Arduino.h>
#include "controller/storage.h"
#include <math.h>
#include <string.h>
#include "controller/joysticks.h"
#include "controller/config.h"

Joystick joyL(JOY_L_PIN_X, JOY_L_PIN_Y, JOY_L_PIN_BTN);
Joystick joyR(JOY_R_PIN_X, JOY_R_PIN_Y, JOY_R_PIN_BTN);

struct CalData
{
    uint16_t magic;
    uint16_t minX;
    uint16_t maxX;
    uint16_t centerX;
    uint16_t minY;
    uint16_t maxY;
    uint16_t centerY;
    uint16_t crc;
};

struct DeadzoneData
{
    uint16_t magic;
    uint16_t dzLX;
    uint16_t dzLY;
    uint16_t dzRX;
    uint16_t dzRY;
    uint16_t crc;
};

struct ExpoData
{
    uint16_t magic;
    float exLX;
    float exLY;
    float exRX;
    float exRY;
    uint16_t crc;
};

struct LimitData
{
    uint16_t magic;
    uint8_t limLX;
    uint8_t limLY;
    uint8_t limRX;
    uint8_t limRY;
    uint16_t crc;
};

static const uint16_t CAL_MAGIC = 0xCA11;
static const uint16_t DEADZONE_MAGIC = 0xD00D;
static const uint16_t EXPO_MAGIC = 0xE202;
static const uint16_t LIMIT_MAGIC = 0x1A17;

static uint16_t crcCal(const CalData &d)
{
    return (uint16_t)(d.magic ^ d.minX ^ d.maxX ^ d.centerX ^ d.minY ^ d.maxY ^ d.centerY ^ 0xA55A);
}

static const char *STORAGE_KEY_JOY_L = "joy_l_cal";
static const char *STORAGE_KEY_JOY_R = "joy_r_cal";
static const char *STORAGE_KEY_DEADZONE = "joy_deadzone";
static const char *STORAGE_KEY_EXPO = "joy_expo";
static const char *STORAGE_KEY_LIMIT = "joy_limit";

static uint16_t crcDeadzone(const DeadzoneData &d)
{
    return (uint16_t)(d.magic ^ d.dzLX ^ d.dzLY ^ d.dzRX ^ d.dzRY ^ 0x5AA5);
}

static int clampDeadzone(int dz)
{
    if (dz < 0)
        dz = 0;
    if (dz > (ADC_MAX * 400 / 1023))
        dz = (ADC_MAX * 400 / 1023);
    return dz;
}

static uint16_t crcExpo(const ExpoData &d)
{
    auto fold = [&](float f) -> uint32_t
    {
        union
        {
            float f;
            uint32_t u;
        } conv;
        conv.f = f;
        return conv.u;
    };
    uint32_t mix = fold(d.exLX) ^ fold(d.exLY) ^ fold(d.exRX) ^ fold(d.exRY) ^ 0xBEEF;
    return (uint16_t)(d.magic ^ ((mix >> 16) ^ (mix & 0xFFFFu)));
}

static float clampExpo(float e)
{
    if (e < 0.0f)
        e = 0.0f;
    if (e > 3.0f)
        e = 3.0f;
    return e;
}

static int clampLimitPct(int pct)
{
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    return pct;
}

static uint16_t crcLimit(const LimitData &d)
{
    return (uint16_t)(d.magic ^ d.limLX ^ d.limLY ^ d.limRX ^ d.limRY ^ 0x6C17);
}

void joystickInit()
{
    analogReadResolution(ADC_BITS);

    joyL.begin();
    joyR.begin();

    joyL.setInvertX(true);
    joyL.setInvertY(false);
    joyR.setInvertX(true);
    joyR.setInvertY(false); 

    joyL.setExpo(JOY_EXPO_DEFAULT);
    joyL.setDeadzone(JOY_DEADZONE_DEFAULT, JOY_DEADZONE_DEFAULT);
    joyL.setLimitPct(100, 100);
    joyR.setExpo(JOY_EXPO_DEFAULT);
    joyR.setDeadzone(JOY_DEADZONE_DEFAULT, JOY_DEADZONE_DEFAULT);
    joyR.setLimitPct(100, 100);

    joysticksLoadCalibration();

    DeadzoneData d{};
    if (storageReadBlob(STORAGE_KEY_DEADZONE, &d, sizeof(d)) &&
        d.magic == DEADZONE_MAGIC && d.crc == crcDeadzone(d))
    {
        int lx = clampDeadzone((int)d.dzLX);
        int ly = clampDeadzone((int)d.dzLY);
        int rx = clampDeadzone((int)d.dzRX);
        int ry = clampDeadzone((int)d.dzRY);
        joyL.setDeadzone(lx, ly);
        joyR.setDeadzone(rx, ry);
    }

    ExpoData ex{};
    if (storageReadBlob(STORAGE_KEY_EXPO, &ex, sizeof(ex)) &&
        ex.magic == EXPO_MAGIC && ex.crc == crcExpo(ex))
    {
        joyL.setExpoX(clampExpo(ex.exLX));
        joyL.setExpoY(clampExpo(ex.exLY));
        joyR.setExpoX(clampExpo(ex.exRX));
        joyR.setExpoY(clampExpo(ex.exRY));
    }

    LimitData lim{};
    if (storageReadBlob(STORAGE_KEY_LIMIT, &lim, sizeof(lim)) &&
        lim.magic == LIMIT_MAGIC && lim.crc == crcLimit(lim))
    {
        joyL.setLimitPct(clampLimitPct((int)lim.limLX), clampLimitPct((int)lim.limLY));
        joyR.setLimitPct(clampLimitPct((int)lim.limRX), clampLimitPct((int)lim.limRY));
    }
}

Joystick::Joystick(uint8_t pinX, uint8_t pinY, uint8_t pinBtn)
    : pinX(pinX), pinY(pinY), pinBtn(pinBtn) {}

void Joystick::begin()
{
    pinMode(pinBtn, INPUT_PULLUP);
}

int Joystick::readAxisRaw(uint8_t pin) const
{
    return analogRead(pin);
}

int Joystick::readRawX() const
{
    return readAxisRaw(pinX);
}

int Joystick::readRawY() const
{
    return readAxisRaw(pinY);
}

int Joystick::readRawInvertedX() const
{
    return applyInvert(readAxisRaw(pinX), true);
}

int Joystick::readRawInvertedY() const
{
    return applyInvert(readAxisRaw(pinY), false);
}

int Joystick::applyInvert(int raw, bool isX) const
{
    return (isX ? invertX : invertY) ? (ADC_MAX - raw) : raw;
}

// ------------------------------------
//       KALIBRACJA
// ------------------------------------
bool Joystick::loadCalibration(const char *key)
{
    CalData d{};
    if (!storageReadBlob(key, &d, sizeof(d)))
        return false;

    if (d.magic != CAL_MAGIC || d.crc != crcCal(d))
        return false;
    if (d.minX >= d.maxX || d.minY >= d.maxY)
        return false;
    if (d.maxX > ADC_MAX || d.maxY > ADC_MAX)
        return false;

    calMinX = d.minX;
    calMaxX = d.maxX;
    calMinY = d.minY;
    calMaxY = d.maxY;
    centerX = d.centerX;
    centerY = d.centerY;

    // make sure center stays within range
    int midX = (calMinX + calMaxX) / 2;
    int midY = (calMinY + calMaxY) / 2;
    if (centerX < calMinX || centerX > calMaxX)
        centerX = midX;
    if (centerY < calMinY || centerY > calMaxY)
        centerY = midY;
    return true;
}

void Joystick::saveCalibration(const char *key)
{
    CalData d{};
    d.magic = CAL_MAGIC;
    d.minX = (uint16_t)calMinX;
    d.maxX = (uint16_t)calMaxX;
    d.centerX = (uint16_t)centerX;
    d.minY = (uint16_t)calMinY;
    d.maxY = (uint16_t)calMaxY;
    d.centerY = (uint16_t)centerY;
    d.crc = crcCal(d);
    storageWriteBlob(key, &d, sizeof(d));
}

void Joystick::startCalibration()
{
    calMinX = calMinY = ADC_MAX;
    calMaxX = calMaxY = 0;
}

void Joystick::updateCalibrationSample()
{
    int rx = applyInvert(readAxisRaw(pinX), true);
    int ry = applyInvert(readAxisRaw(pinY), false);

    if (rx < calMinX)
        calMinX = rx;
    if (rx > calMaxX)
        calMaxX = rx;
    if (ry < calMinY)
        calMinY = ry;
    if (ry > calMaxY)
        calMaxY = ry;
}

void Joystick::finishCalibration()
{
    // zabezpieczenie gdy nie ruszono drÄ…ĹĽkiem
    if (calMaxX <= calMinX + 2)
    {
        calMinX = 0;
        calMaxX = ADC_MAX;
    }
    if (calMaxY <= calMinY + 2)
    {
        calMinY = 0;
        calMaxY = ADC_MAX;
    }

    centerX = (calMinX + calMaxX) / 2;
    centerY = (calMinY + calMaxY) / 2;
}

void Joystick::setCalibration(int minX, int maxX, int minY, int maxY)
{
    calMinX = (minX < 0) ? 0 : (minX > ADC_MAX ? ADC_MAX : minX);
    calMaxX = (maxX < 0) ? 0 : (maxX > ADC_MAX ? ADC_MAX : maxX);
    calMinY = (minY < 0) ? 0 : (minY > ADC_MAX ? ADC_MAX : minY);
    calMaxY = (maxY < 0) ? 0 : (maxY > ADC_MAX ? ADC_MAX : maxY);

    if (calMaxX <= calMinX + 2)
    {
        calMinX = 0;
        calMaxX = ADC_MAX;
    }
    if (calMaxY <= calMinY + 2)
    {
        calMinY = 0;
        calMaxY = ADC_MAX;
    }

    centerX = (calMinX + calMaxX) / 2;
    centerY = (calMinY + calMaxY) / 2;
}

void joysticksLoadCalibration()
{
    bool okL = joyL.loadCalibration(STORAGE_KEY_JOY_L);
    bool okR = joyR.loadCalibration(STORAGE_KEY_JOY_R);
    if (!okL)
    {
        joyL.finishCalibration(); // ustawia default range
    }
    if (!okR)
    {
        joyR.finishCalibration();
    }
}

void joysticksSaveCalibration()
{
    joyL.saveCalibration(STORAGE_KEY_JOY_L);
    joyR.saveCalibration(STORAGE_KEY_JOY_R);
}

int joysticksGetDeadzoneAxis(uint8_t axis)
{
    switch (axis)
    {
    case 0:
        return joyL.getDeadzoneX();
    case 1:
        return joyL.getDeadzoneY();
    case 2:
        return joyR.getDeadzoneX();
    case 3:
        return joyR.getDeadzoneY();
    default:
        return 0;
    }
}

void joysticksSetDeadzoneAxis(uint8_t axis, int dz)
{
    int c = clampDeadzone(dz);
    switch (axis)
    {
    case 0:
        joyL.setDeadzone(c, joyL.getDeadzoneY());
        break;
    case 1:
        joyL.setDeadzone(joyL.getDeadzoneX(), c);
        break;
    case 2:
        joyR.setDeadzone(c, joyR.getDeadzoneY());
        break;
    case 3:
        joyR.setDeadzone(joyR.getDeadzoneX(), c);
        break;
    default:
        break;
    }
}

void joysticksSaveDeadzone()
{
    DeadzoneData d{};
    d.magic = DEADZONE_MAGIC;
    d.dzLX = (uint16_t)clampDeadzone(joyL.getDeadzoneX());
    d.dzLY = (uint16_t)clampDeadzone(joyL.getDeadzoneY());
    d.dzRX = (uint16_t)clampDeadzone(joyR.getDeadzoneX());
    d.dzRY = (uint16_t)clampDeadzone(joyR.getDeadzoneY());
    d.crc = crcDeadzone(d);
    storageWriteBlob(STORAGE_KEY_DEADZONE, &d, sizeof(d));
}

float joysticksGetExpoAxis(uint8_t axis)
{
    switch (axis)
    {
    case 0:
        return joyL.getExpoX();
    case 1:
        return joyL.getExpoY();
    case 2:
        return joyR.getExpoX();
    case 3:
        return joyR.getExpoY();
    default:
        return 0.0f;
    }
}

void joysticksSetExpoAxis(uint8_t axis, float e)
{
    float c = clampExpo(e);
    switch (axis)
    {
    case 0:
        joyL.setExpoX(c);
        break;
    case 1:
        joyL.setExpoY(c);
        break;
    case 2:
        joyR.setExpoX(c);
        break;
    case 3:
        joyR.setExpoY(c);
        break;
    default:
        break;
    }
}

void joysticksSaveExpoAxis(uint8_t axis)
{
    ExpoData d{};
    bool valid = storageReadBlob(STORAGE_KEY_EXPO, &d, sizeof(d)) &&
                 d.magic == EXPO_MAGIC && d.crc == crcExpo(d);
    if (!valid)
    {
        d.magic = EXPO_MAGIC;
        d.exLX = clampExpo(joyL.getExpoX());
        d.exLY = clampExpo(joyL.getExpoY());
        d.exRX = clampExpo(joyR.getExpoX());
        d.exRY = clampExpo(joyR.getExpoY());
    }

    switch (axis)
    {
    case 0: d.exLX = clampExpo(joyL.getExpoX()); break;
    case 1: d.exLY = clampExpo(joyL.getExpoY()); break;
    case 2: d.exRX = clampExpo(joyR.getExpoX()); break;
    case 3: d.exRY = clampExpo(joyR.getExpoY()); break;
    default: break;
    }

    d.crc = crcExpo(d);
    storageWriteBlob(STORAGE_KEY_EXPO, &d, sizeof(d));
}

int joysticksGetLimitAxis(uint8_t axis)
{
    switch (axis)
    {
    case 0:
        return joyL.getLimitPctX();
    case 1:
        return joyL.getLimitPctY();
    case 2:
        return joyR.getLimitPctX();
    case 3:
        return joyR.getLimitPctY();
    default:
        return 100;
    }
}

void joysticksSetLimitAxis(uint8_t axis, int pct)
{
    const int c = clampLimitPct(pct);
    switch (axis)
    {
    case 0:
        joyL.setLimitPctX(c);
        break;
    case 1:
        joyL.setLimitPctY(c);
        break;
    case 2:
        joyR.setLimitPctX(c);
        break;
    case 3:
        joyR.setLimitPctY(c);
        break;
    default:
        break;
    }
}

void joysticksSaveLimit()
{
    LimitData d{};
    d.magic = LIMIT_MAGIC;
    d.limLX = (uint8_t)clampLimitPct(joyL.getLimitPctX());
    d.limLY = (uint8_t)clampLimitPct(joyL.getLimitPctY());
    d.limRX = (uint8_t)clampLimitPct(joyR.getLimitPctX());
    d.limRY = (uint8_t)clampLimitPct(joyR.getLimitPctY());
    d.crc = crcLimit(d);
    storageWriteBlob(STORAGE_KEY_LIMIT, &d, sizeof(d));
}

float Joystick::processAxis(int raw, const char *axisName)
{
    raw = constrain(raw, 0, ADC_MAX);

    bool isX = (axisName && axisName[0] == 'X');

    raw = applyInvert(raw, isX);

    int calMin = isX ? calMinX : calMinY;
    int calMax = isX ? calMaxX : calMaxY;
    int calCenter = isX ? centerX : centerY;
    if (calMax <= calMin + 2)
    {
        calMin = 0;
        calMax = ADC_MAX;
    } // fallback gdy brak kalibracji
    if (calCenter < calMin || calCenter > calMax)
        calCenter = (calMin + calMax) / 2;

    int dz = isX ? deadzoneX : deadzoneY;

    float mid = (float)calCenter;
    float spanPos = (float)(calMax - calCenter);
    float spanNeg = (float)(calCenter - calMin);
    if (spanPos < 1.0f)
        spanPos = 1.0f;
    if (spanNeg < 1.0f)
        spanNeg = 1.0f;

    float centered = raw - mid;
    float absC = fabs(centered);
    float sign = (centered >= 0) ? 1.0f : -1.0f;

    float xMax = (sign >= 0.0f) ? spanPos : spanNeg;

    if (absC <= dz || xMax <= dz)
    {
        return 0.0f;
    }

    float x = absC;
    float norm = (x - dz) / (xMax - dz);
    if (norm < 0.0f)
        norm = 0.0f;
    if (norm > 1.0f)
        norm = 1.0f;

    float expoVal = isX ? expoX : expoY;
    float curved = 100.0f * powf(norm, 1.0f + expoVal);
    if (curved > 100.0f)
        curved = 100.0f;

    const float limitPct = (float)(isX ? limitPctX : limitPctY);
    curved = curved * constrain(limitPct, 0.0f, 100.0f) / 100.0f;

    return sign * curved;
}

float Joystick::processAxisLinear(int raw, const char *axisName) const
{
    raw = constrain(raw, 0, ADC_MAX);

    bool isX = (axisName && axisName[0] == 'X');
    raw = applyInvert(raw, isX);

    int calMin = isX ? calMinX : calMinY;
    int calMax = isX ? calMaxX : calMaxY;
    int calCenter = isX ? centerX : centerY;
    if (calMax <= calMin + 2)
    {
        calMin = 0;
        calMax = ADC_MAX;
    }
    if (calCenter < calMin || calCenter > calMax)
        calCenter = (calMin + calMax) / 2;

    float spanPos = (float)(calMax - calCenter);
    float spanNeg = (float)(calCenter - calMin);
    if (spanPos < 1.0f)
        spanPos = 1.0f;
    if (spanNeg < 1.0f)
        spanNeg = 1.0f;

    float centered = (float)raw - (float)calCenter;
    float sign = (centered >= 0.0f) ? 1.0f : -1.0f;
    float span = (sign >= 0.0f) ? spanPos : spanNeg;
    float norm = fabsf(centered) / span;
    if (norm > 1.0f)
        norm = 1.0f;

    return sign * norm * 100.0f;
}

float Joystick::readX()
{
    int raw = readAxisRaw(pinX);
    return processAxis(raw, "X");
}

float Joystick::readY()
{
    int raw = readAxisRaw(pinY);
    return processAxis(raw, "Y");
}

float Joystick::readLinearX()
{
    int raw = readAxisRaw(pinX);
    return processAxisLinear(raw, "X");
}

float Joystick::readLinearY()
{
    int raw = readAxisRaw(pinY);
    return processAxisLinear(raw, "Y");
}
