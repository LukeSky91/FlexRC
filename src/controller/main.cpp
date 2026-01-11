#include "ide_compat.h"
#include <Arduino.h>
#include <Wire.h>
#include "common/comm.h"
#include "common/time_utils.h"
#include "controller/config.h"
#include "controller/display.h"
#include "controller/buttons.h"
#include "controller/leds.h"
#include "controller/joysticks.h"
#include "controller/ui/menu.h"
#include "controller/receiver.h"

int mode = 0;
static uint8_t batState = 0;

static int8_t mapToPct(int16_t v)
{
    long scaled = (long)v * 100 / 32767;
    if (scaled > 100)
        scaled = 100;
    if (scaled < -100)
        scaled = -100;
    return (int8_t)scaled;
}

void setup()
{
    Serial.begin(115200);

    // ===== I2C globalnie =====
    // Mega: SDA=20, SCL=21. Nano: A4/A5.
    Wire.begin();
    Wire.setClock(400000);

    // Timeout w µs (na AVR): 50ms => 50000
    // reset=false: NIE resetuj sprzŽttowego TWI w ‘>rodku transakcji (u8g2/SH1106 tego nie lubi).
    Wire.setWireTimeout(50000, false);

    // ===== Inity peryferiÆˆw =====
    displayInit(); // tylko inicjalizacja wy‘>wietlacza (bez blokowania sterowania w loop)
    buttonsInit();
    ledsInit();
    joystickInit();

#if EEPROM_FORCE_DEFAULTS_ON_BOOT
    // Force default config into EEPROM (use once after upload).
    joyL.setDeadzone(JOY_DEADZONE_DEFAULT, JOY_DEADZONE_DEFAULT);
    joyR.setDeadzone(JOY_DEADZONE_DEFAULT, JOY_DEADZONE_DEFAULT);
    joyL.setExpo(JOY_EXPO_DEFAULT);
    joyR.setExpo(JOY_EXPO_DEFAULT);
    joyL.setCalibration(0, 1023, 0, 1023);
    joyR.setCalibration(0, 1023, 0, 1023);
    joyL.setCenter(512, 512);
    joyR.setCenter(512, 512);
    joysticksSaveCalibration();
    joysticksSaveDeadzone();
    joysticksSaveExpo();

    buttonsSetThreshold(Key::Down, TH_DOWN_DEFAULT);
    buttonsSetThreshold(Key::Up, TH_UP_DEFAULT);
    buttonsSetThreshold(Key::Right, TH_RIGHT_DEFAULT);
    buttonsSetThreshold(Key::Center, TH_CENTER_DEFAULT);
    buttonsSetThreshold(Key::Left, TH_LEFT_DEFAULT);
    buttonsSaveThresholds();
#endif

    ledsSet(LedSlot::First, RED, 100);
    ledsSet(LedSlot::Second, RED, 100);
    ledsSet(LedSlot::Third, RED, 100);
    ledsShow();

    static const uint8_t NRF_ADDR[5] = {'R', 'C', '0', '0', '1'};
    commInit(NRF_CE_PIN, NRF_CSN_PIN, NRF_CHANNEL, NRF_ADDR);
    receiverInit();

    menuInit();

    // ===== Ekran startowy (tylko ustawia bufory; faktyczny render zrobi displayTick()) =====
    displayClear();
    displayText(0, "BOOT OK");

#if PERF_DEBUG
    Serial.println("Start");
#endif
}

void loop()
{

    buttonsTick();

#if PERF_DEBUG
    uint32_t t0 = millis();
#endif

    bool inCalib = menuLoop(mode, batState);

    // 2) komunikacja / sterowanie (zawsze, je‘>li nie jeste‘> w kalibracji)
    if (!inCalib)
    {
        CommFrame tx{
            mapToPct(joyL.readX()),
            mapToPct(joyL.readY()),
            mapToPct(joyR.readX()),
            mapToPct(joyR.readY()),
            0u};
        receiverLoop(tx);
    }

    static uint32_t ledShowTick = 0;
    if (everyMs(20, ledShowTick))
    {
        ledsShow();
    }

    displayTick();

#if PERF_DEBUG
    uint32_t t1 = millis();
    if (t1 - t0 > 20)
    {
        Serial.print("[SLOW] loop ms=");
        Serial.println(t1 - t0);
    }
#endif
}
