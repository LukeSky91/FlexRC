#include <Arduino.h>
#include <EEPROM.h>
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

static const uint16_t CAL_MAGIC = 0xCA11;
static const uint16_t DEADZONE_MAGIC = 0xD00D;
static const uint16_t EXPO_MAGIC = 0xE202;

static uint16_t crcCal(const CalData &d)
{
    return (uint16_t)(d.magic ^ d.minX ^ d.maxX ^ d.centerX ^ d.minY ^ d.maxY ^ d.centerY ^ 0xA55A);
}

static const uint16_t EEPROM_ADDR_L = 0;
static const uint16_t EEPROM_ADDR_R = EEPROM_ADDR_L + sizeof(CalData);
static const uint16_t EEPROM_ADDR_DEADZONE = EEPROM_ADDR_R + sizeof(CalData);
static const uint16_t EEPROM_ADDR_EXPO = EEPROM_ADDR_DEADZONE + sizeof(DeadzoneData);
static const uint16_t EEPROM_ADDR_AFTER_EXPO = EEPROM_ADDR_EXPO + sizeof(ExpoData);

static uint16_t crcDeadzone(const DeadzoneData &d)
{
    return (uint16_t)(d.magic ^ d.dzLX ^ d.dzLY ^ d.dzRX ^ d.dzRY ^ 0x5AA5);
}

static int clampDeadzone(int dz)
{
    if (dz < 0)
        dz = 0;
    if (dz > 400)
        dz = 400;
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

void joystickInit()
{
    joyL.begin();
    joyR.begin();

    // X increases to the right, Y increases upward (no Y inversion)
    joyL.setInvertX(true);
    joyL.setInvertY(false);
    joyR.setInvertX(true);
    joyR.setInvertY(false);

    joyL.setExpo(JOY_EXPO_DEFAULT);
    joyL.setDeadzone(JOY_DEADZONE_DEFAULT, JOY_DEADZONE_DEFAULT);
    joyR.setExpo(JOY_EXPO_DEFAULT);
    joyR.setDeadzone(JOY_DEADZONE_DEFAULT, JOY_DEADZONE_DEFAULT);

    joysticksLoadCalibration();

    DeadzoneData d{};
    EEPROM.get(EEPROM_ADDR_DEADZONE, d);
    if (d.magic == DEADZONE_MAGIC && d.crc == crcDeadzone(d))
    {
        int lx = clampDeadzone((int)d.dzLX);
        int ly = clampDeadzone((int)d.dzLY);
        int rx = clampDeadzone((int)d.dzRX);
        int ry = clampDeadzone((int)d.dzRY);
        joyL.setDeadzone(lx, ly);
        joyR.setDeadzone(rx, ry);
    }

    ExpoData ex{};
    EEPROM.get(EEPROM_ADDR_EXPO, ex);
    if (ex.magic == EXPO_MAGIC && ex.crc == crcExpo(ex))
    {
        joyL.setExpoX(clampExpo(ex.exLX));
        joyL.setExpoY(clampExpo(ex.exLY));
        joyR.setExpoX(clampExpo(ex.exRX));
        joyR.setExpoY(clampExpo(ex.exRY));
    }
}

Joystick::Joystick(uint8_t pinX, uint8_t pinY, uint8_t pinBtn)
    : pinX(pinX), pinY(pinY), pinBtn(pinBtn) {}

void Joystick::begin()
{
    pinMode(pinBtn, INPUT_PULLUP);
}

bool Joystick::pressed()
{
    return digitalRead(pinBtn) == LOW;
}

int Joystick::readAxisRaw(uint8_t pin) const
{
    return analogRead(pin);
}

int Joystick::readRawX() const
{
    // return raw in the same orientation as processing (honor invertX)
    return applyInvert(readAxisRaw(pinX), true);
}

int Joystick::readRawY() const
{
    // return raw in the same orientation as processing (honor invertY)
    return applyInvert(readAxisRaw(pinY), false);
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
    return (isX ? invertX : invertY) ? (1023 - raw) : raw;
}

// ------------------------------------
//       KALIBRACJA
// ------------------------------------
bool Joystick::loadCalibration(uint16_t addr)
{
    CalData d{};
    EEPROM.get(addr, d);

    if (d.magic != CAL_MAGIC || d.crc != crcCal(d))
        return false;
    if (d.minX >= d.maxX || d.minY >= d.maxY)
        return false;
    if (d.maxX > 1023 || d.maxY > 1023)
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

void Joystick::saveCalibration(uint16_t addr)
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
    EEPROM.put(addr, d);
}

void Joystick::startCalibration()
{
    calMinX = calMinY = 1023;
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
        calMaxX = 1023;
    }
    if (calMaxY <= calMinY + 2)
    {
        calMinY = 0;
        calMaxY = 1023;
    }

    centerX = (calMinX + calMaxX) / 2;
    centerY = (calMinY + calMaxY) / 2;
}

void Joystick::setCalibration(int minX, int maxX, int minY, int maxY)
{
    calMinX = (minX < 0) ? 0 : (minX > 1023 ? 1023 : minX);
    calMaxX = (maxX < 0) ? 0 : (maxX > 1023 ? 1023 : maxX);
    calMinY = (minY < 0) ? 0 : (minY > 1023 ? 1023 : minY);
    calMaxY = (maxY < 0) ? 0 : (maxY > 1023 ? 1023 : maxY);

    if (calMaxX <= calMinX + 2)
    {
        calMinX = 0;
        calMaxX = 1023;
    }
    if (calMaxY <= calMinY + 2)
    {
        calMinY = 0;
        calMaxY = 1023;
    }

    centerX = (calMinX + calMaxX) / 2;
    centerY = (calMinY + calMaxY) / 2;
}

void Joystick::recenterAround(int centerX, int centerY)
{
    int spanX = calMaxX - calMinX;
    int spanY = calMaxY - calMinY;
    if (spanX < 2)
        spanX = 1023;
    if (spanY < 2)
        spanY = 1023;

    int halfX = spanX / 2;
    int halfY = spanY / 2;

    calMinX = centerX - halfX;
    calMaxX = centerX + halfX;
    calMinY = centerY - halfY;
    calMaxY = centerY + halfY;

    if (calMinX < 0)
        calMinX = 0;
    if (calMaxX > 1023)
        calMaxX = 1023;
    if (calMinY < 0)
        calMinY = 0;
    if (calMaxY > 1023)
        calMaxY = 1023;

    if (calMaxX <= calMinX + 2)
    {
        calMinX = 0;
        calMaxX = 1023;
    }
    if (calMaxY <= calMinY + 2)
    {
        calMinY = 0;
        calMaxY = 1023;
    }

    this->centerX = (centerX < calMinX) ? calMinX : (centerX > calMaxX ? calMaxX : centerX);
    this->centerY = (centerY < calMinY) ? calMinY : (centerY > calMaxY ? calMaxY : centerY);
}

void joysticksLoadCalibration()
{
    bool okL = joyL.loadCalibration(EEPROM_ADDR_L);
    bool okR = joyR.loadCalibration(EEPROM_ADDR_R);
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
    joyL.saveCalibration(EEPROM_ADDR_L);
    joyR.saveCalibration(EEPROM_ADDR_R);
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
    EEPROM.put(EEPROM_ADDR_DEADZONE, d);
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

void joysticksSaveExpo()
{
    ExpoData d{};
    d.magic = EXPO_MAGIC;
    d.exLX = clampExpo(joyL.getExpoX());
    d.exLY = clampExpo(joyL.getExpoY());
    d.exRX = clampExpo(joyR.getExpoX());
    d.exRY = clampExpo(joyR.getExpoY());
    d.crc = crcExpo(d);
    EEPROM.put(EEPROM_ADDR_EXPO, d);
}

void joysticksSaveExpoAxis(uint8_t axis)
{
    ExpoData d{};
    EEPROM.get(EEPROM_ADDR_EXPO, d);
    bool valid = (d.magic == EXPO_MAGIC && d.crc == crcExpo(d));
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
    EEPROM.put(EEPROM_ADDR_EXPO, d);
}

uint16_t joysticksEepromAddrAfterExpo()
{
    return EEPROM_ADDR_AFTER_EXPO;
}

int16_t Joystick::processAxis(int raw, const char *axisName)
{

    raw = constrain(raw, 0, 1023);

    bool isX = (axisName && axisName[0] == 'X');

    raw = applyInvert(raw, isX);

    int calMin = isX ? calMinX : calMinY;
    int calMax = isX ? calMaxX : calMaxY;
    int calCenter = isX ? centerX : centerY;
    if (calMax <= calMin + 2)
    {
        calMin = 0;
        calMax = 1023;
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

    constexpr float mid_16b = 32767.0f; // srodek 16-bit (65535/2)
    float centered = raw - mid;
    float absC = fabs(centered);
    float sign = (centered >= 0) ? 1.0f : -1.0f;

    float xMax = (sign >= 0.0f) ? spanPos : spanNeg;

    if (absC <= dz || xMax <= dz)
    {
        return 0;
    }

    float x = absC;
    float norm = (x - dz) / (xMax - dz);
    if (norm < 0.0f)
        norm = 0.0f;
    if (norm > 1.0f)
        norm = 1.0f;

    float expoVal = isX ? expoX : expoY;
    float curved = roundf(mid_16b * pow(norm, 1.0f + expoVal));
    if (curved > mid_16b)
        curved = mid_16b;

    int16_t out = (int16_t)(sign * curved);

    return out;
}

int16_t Joystick::readX()
{
    int raw = readAxisRaw(pinX);
    return processAxis(raw, "X");
}

int16_t Joystick::readY()
{
    int raw = readAxisRaw(pinY);
    return processAxis(raw, "Y");
}
