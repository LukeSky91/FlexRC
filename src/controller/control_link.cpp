#include <Arduino.h>

#include "controller/buttons.h"
#include "controller/control_link.h"
#include "controller/leds.h"
#include "controller/photo_sensor.h"
#include "controller/receiver.h"
#include "controller/ui/loop_main.h"
#include "common/time_utils.h"

static const uint32_t LINK_TOGGLE_HOLD_MS = 1200;
static const uint32_t ARM_TOGGLE_HOLD_MS = 1200;
static const uint32_t ARM_DENIED_BLINK_MS = 250;
static const uint32_t ARM_DENIED_OLED_MS = 2000;

static bool gControlArmed = false;
static uint32_t gArmDeniedBlinkUntilMs = 0;
static uint32_t gArmDeniedUntilMs = 0;
static bool gWasInMainLoop = true;

static void setControlArmed(bool armed)
{
    gControlArmed = armed;
    screenMainSetArmState(gControlArmed ? DashboardArmState::Armed : DashboardArmState::Safe);
}

static void denyArm()
{
    gArmDeniedBlinkUntilMs = millis() + ARM_DENIED_BLINK_MS;
    gArmDeniedUntilMs = millis() + ARM_DENIED_OLED_MS;
    screenMainSetArmState(DashboardArmState::NoPermission);
}

static void updateArmLed()
{
    static uint32_t ledStatusTick = 0;
    if (!everyMs(50, ledStatusTick))
        return;

    const uint8_t brightnessPct = photoSensorLedBrightnessPct();
    Color armLed = gControlArmed ? WHITE : AMBER;
    if (gArmDeniedBlinkUntilMs > millis())
        armLed = RED;
    ledsSet(LedSlot::Second, armLed, brightnessPct);
}

void controlLinkInit()
{
    gControlArmed = false;
    gArmDeniedBlinkUntilMs = 0;
    gArmDeniedUntilMs = 0;
    gWasInMainLoop = true;
    screenMainSetArmState(DashboardArmState::Safe);
}

void controlLinkTick(bool inMainLoop)
{
    if (!gControlArmed && gArmDeniedUntilMs != 0 && millis() >= gArmDeniedUntilMs)
    {
        gArmDeniedUntilMs = 0;
        screenMainSetArmState(DashboardArmState::Safe);
    }

    if (gWasInMainLoop && !inMainLoop)
    {
        setControlArmed(false);
        gArmDeniedUntilMs = 0;
    }
    gWasInMainLoop = inMainLoop;

    const ReceiverLinkState linkState = receiverGetLinkState();
    if (gControlArmed &&
        (linkState == ReceiverLinkState::Idle ||
         linkState == ReceiverLinkState::Lost ||
         linkState == ReceiverLinkState::RadioError))
    {
        setControlArmed(false);
        gArmDeniedUntilMs = 0;
    }

    if (inMainLoop && keyLongPress(Key::F1, false, 300, LINK_TOGGLE_HOLD_MS))
    {
        receiverSetLinkEnabled(!receiverIsLinkEnabled());
        if (!receiverIsLinkEnabled())
            setControlArmed(false);
    }

    if (inMainLoop && keyLongPress(Key::F2, false, 300, ARM_TOGGLE_HOLD_MS))
    {
        if (receiverGetLinkState() == ReceiverLinkState::Connected)
        {
            setControlArmed(!gControlArmed);
        }
        else
        {
            denyArm();
            gControlArmed = false;
        }
    }

    updateArmLed();
}

bool controlLinkAllowsLiveControls(bool inMainLoop)
{
    return inMainLoop &&
           gControlArmed &&
           receiverGetLinkState() == ReceiverLinkState::Connected;
}
