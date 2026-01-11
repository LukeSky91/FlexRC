#pragma once
#include <Arduino.h>

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
void joysticksSaveExpo();

// EEPROM layout helper: address after Expo block (for other modules)
uint16_t joysticksEepromAddrAfterExpo();

class Joystick {
public:
    Joystick(uint8_t pinX, uint8_t pinY, uint8_t pinBtn);

    void begin();
    bool pressed();

    int16_t readX();
    int16_t readY();

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
    void setCenter(int cx, int cy) { centerX = cx; centerY = cy; }
    int getCenterX() const { return centerX; }
    int getCenterY() const { return centerY; }

    // Calibration
    void startCalibration();
    void updateCalibrationSample();
    void finishCalibration();
    bool loadCalibration(uint16_t addr);
    void saveCalibration(uint16_t addr);
    int getCalMinX() const { return calMinX; }
    int getCalMaxX() const { return calMaxX; }
    int getCalMinY() const { return calMinY; }
    int getCalMaxY() const { return calMaxY; }
    void setCalibration(int minX, int maxX, int minY, int maxY);
    void recenterAround(int centerX, int centerY);

    // Raw ADC reads (no calibration/curve/expo applied)
    int readRawX() const;
    int readRawY() const;
    int readRawInvertedX() const;
    int readRawInvertedY() const;

private:
    uint8_t pinX, pinY, pinBtn;

    bool invertX = false;
    bool invertY = false;
    int deadzoneX = 40;
    int deadzoneY = 40;
    float expoX = 1.8f;
    float expoY = 1.8f;

    int calMinX = 0, calMaxX = 1023;
    int calMinY = 0, calMaxY = 1023;
    int centerX = 512, centerY = 512;

    int readAxisRaw(uint8_t pin) const;
    int applyInvert(int raw, bool isX) const;
    int16_t processAxis(int raw, const char* axisName);
};

// Global instances of left and right joysticks
extern Joystick joyL;
extern Joystick joyR;
