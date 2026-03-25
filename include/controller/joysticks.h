#pragma once
#include <Arduino.h>
#include "controller/config.h"

// Initialize both joysticks
void joystickInit();
void joysticksLoadCalibration();
void joysticksSaveCalibration();

// Deadzone per axis: 0=lx,1=ly,2=rx,3=ry
int joysticksGetDeadzoneAxis(uint8_t axis);
void joysticksSetDeadzoneAxis(uint8_t axis, int dz);
void joysticksSaveDeadzone();

// Expo per axis: 0=lx,1=ly,2=rx,3=ry
float joysticksGetExpoAxis(uint8_t axis);
void joysticksSetExpoAxis(uint8_t axis, float e);
void joysticksSaveExpoAxis(uint8_t axis);

// Limit per axis in percent: 0=lx,1=ly,2=rx,3=ry
int joysticksGetLimitAxis(uint8_t axis);
void joysticksSetLimitAxis(uint8_t axis, int pct);
void joysticksSaveLimit();

class Joystick {
public:
    Joystick(uint8_t pinX, uint8_t pinY, uint8_t pinBtn);

    void begin();

    float readX();
    float readY();
    float readLinearX();
    float readLinearY();

    void setInvertX(bool b) { invertX = b; }
    void setInvertY(bool b) { invertY = b; }

    void setDeadzone(int dzX, int dzY) { deadzoneX = dzX; deadzoneY = dzY; }
    int getDeadzoneX() const { return deadzoneX; }
    int getDeadzoneY() const { return deadzoneY; }
    void setExpo(float e) { expoX = e; expoY = e; }
    void setExpoX(float e) { expoX = e; }
    void setExpoY(float e) { expoY = e; }
    float getExpoX() const { return expoX; }
    float getExpoY() const { return expoY; }
    void setLimitPct(int pctX, int pctY) { limitPctX = pctX; limitPctY = pctY; }
    void setLimitPctX(int pct) { limitPctX = pct; }
    void setLimitPctY(int pct) { limitPctY = pct; }
    int getLimitPctX() const { return limitPctX; }
    int getLimitPctY() const { return limitPctY; }
    void setCenter(int cx, int cy) { centerX = cx; centerY = cy; }
    int getCenterX() const { return centerX; }
    int getCenterY() const { return centerY; }

    // Calibration
    void startCalibration();
    void updateCalibrationSample();
    void finishCalibration();
    bool loadCalibration(const char *key);
    void saveCalibration(const char *key);
    int getCalMinX() const { return calMinX; }
    int getCalMaxX() const { return calMaxX; }
    int getCalMinY() const { return calMinY; }
    int getCalMaxY() const { return calMaxY; }
    void setCalibration(int minX, int maxX, int minY, int maxY);

    // Raw ADC reads (physical ADC, no inversion/calibration/curve applied)
    int readRawX() const;
    int readRawY() const;
    // Raw ADC reads in control orientation (with inversion, no calibration/curve applied)
    int readRawInvertedX() const;
    int readRawInvertedY() const;

private:
    uint8_t pinX, pinY, pinBtn;

    bool invertX = false;
    bool invertY = false;
    int deadzoneX = JOY_DEADZONE_DEFAULT;
    int deadzoneY = JOY_DEADZONE_DEFAULT;
    float expoX = 1.8f;
    float expoY = 1.8f;
    int limitPctX = 100;
    int limitPctY = 100;

    int calMinX = 0, calMaxX = ADC_MAX;
    int calMinY = 0, calMaxY = ADC_MAX;
    int centerX = ADC_CENTER, centerY = ADC_CENTER;

    int readAxisRaw(uint8_t pin) const;
    int applyInvert(int raw, bool isX) const;
    float processAxisLinear(int raw, const char *axisName) const;
    float processAxis(int raw, const char* axisName);
};

// Global instances of left and right joysticks
extern Joystick joyL;
extern Joystick joyR;
