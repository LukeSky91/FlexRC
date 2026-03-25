#include "ide_compat.h"
#include <Arduino.h>
#include <Wire.h>
#include "common/comm.h"
#include "common/time_utils.h"
#include "controller/config.h"
#include "controller/display.h"
#include "controller/buttons.h"
#include "controller/leds.h"
#include "controller/control_link.h"
#include "controller/joysticks.h"
#include "controller/photo_sensor.h"
#include "controller/storage.h"
#include "controller/battery.h"
#include "controller/tx_frame.h"
#include "controller/ui/menu.h"
#include "controller/receiver.h"

int mode = 0;
static uint8_t batState = 0;

void setup()
{
    Serial.begin(115200);
    storageInit();

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_CLOCK_HZ);
    Wire.setClock(I2C_CLOCK_HZ);

    displayInit();
    buttonsInit();
    ledsInit();
    joystickInit();
    photoSensorInit();
    batteryInit();

#if EEPROM_FORCE_DEFAULTS_ON_BOOT
    // Force default config into EEPROM (use once after upload).
    joyL.setDeadzone(JOY_DEADZONE_DEFAULT, JOY_DEADZONE_DEFAULT);
    joyR.setDeadzone(JOY_DEADZONE_DEFAULT, JOY_DEADZONE_DEFAULT);
    joyL.setExpo(JOY_EXPO_DEFAULT);
    joyR.setExpo(JOY_EXPO_DEFAULT);
    joyL.setCalibration(0, ADC_MAX, 0, ADC_MAX);
    joyR.setCalibration(0, ADC_MAX, 0, ADC_MAX);
    joyL.setCenter(ADC_CENTER, ADC_CENTER);
    joyR.setCenter(ADC_CENTER, ADC_CENTER);
    joysticksSaveCalibration();
    joysticksSaveDeadzone();
    joysticksSaveExpoAxis(0);
    joysticksSaveExpoAxis(1);
    joysticksSaveExpoAxis(2);
    joysticksSaveExpoAxis(3);
#endif

    ledsSet(LedSlot::First, RED, 100);
    ledsSet(LedSlot::Second, RED, 100);
    ledsSet(LedSlot::Third, RED, 100);
    ledsShow();

    static const uint8_t NRF_ADDR[5] = {'R', 'C', '0', '0', '1'};
    const bool radioReady = commInit(NRF_CE_PIN, NRF_CSN_PIN, NRF_CHANNEL, NRF_ADDR);
    receiverInit(radioReady);

    menuInit();
    controlLinkInit();

    displayClear();
    displayText(0, "BOOT OK");

#if PERF_DEBUG
    Serial.println("Start");
#endif

    if (!radioReady)
    {
        Serial.println("[RADIO] init failed");
    }
}

void loop()
{
    buttonsTick();
    batteryTick();

#if PERF_DEBUG
    uint32_t t0 = millis();
#endif

    bool inCalib = menuLoop(mode, batState);
    const bool inMainLoop = menuIsInMainLoop();
    controlLinkTick(inMainLoop);
    receiverLoop(txFrameBuild(!inCalib && controlLinkAllowsLiveControls(inMainLoop)));

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
