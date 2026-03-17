#include <Arduino.h>
#include "common/time_utils.h"
#include "controller/ui/loop_main.h"
#include "controller/joysticks.h"
#include "controller/photo_sensor.h"
#include "controller/leds.h"
#include "controller/receiver.h"
#include "controller/battery.h"
#include "controller/buttons.h"
#include "controller/ui/menu.h"
#include "controller/config.h"

static uint32_t oledTick = 0;
static uint8_t page = 1; // 1=MAIN, 2=L, 3=R, 4=PHOTO, 5=SETTINGS
static const uint8_t totalPages = 5;
static bool splashInit = false;
static bool splashActive = true;
static uint32_t splashUntilMs = 0;
static bool settingsRequested = false;
static bool settingsArmed = false;
static uint8_t prevPage = 1;

static int16_t displayPct(float v)
{
    if (v > 100.0f)
        v = 100.0f;
    if (v < -100.0f)
        v = -100.0f;
    return (int16_t)((v >= 0.0f) ? (v + 0.5f) : (v - 0.5f));
}

void screenMainSetStartPage(uint8_t startPage, bool skipSplash)
{
    if (startPage < 1 || startPage > totalPages)
        startPage = 1;
    page = startPage;
    prevPage = page;
    settingsArmed = false;
    settingsRequested = false;
    splashInit = true;
    splashActive = !skipSplash;
    splashUntilMs = skipSplash ? 0 : (millis() + 2000);
}

void screenMainLoop(int mode, uint8_t batState)
{
    (void)mode;
    (void)batState;

    bool pageChanged = false;

    if (!splashInit)
    {
        splashInit = true;
        splashActive = true;
        splashUntilMs = millis() + 2000; // show startup page for ~2s
        page = 1;
        prevPage = page;
        settingsArmed = false;
        settingsRequested = false;
        pageChanged = true; // first render
    }

    if (splashActive && millis() >= splashUntilMs)
    {
        splashActive = false;
        pageChanged = true;
    }

    // LEFT/RIGHT: click or hold with auto-repeat
    if (!splashActive && (keyShortClick(Key::Right) || keyLongPress(Key::Right, true)))
    {
        page = (uint8_t)(((page - 1 + 1) % totalPages) + 1); // cycle 1..totalPages
        pageChanged = true;
    }
    else if (!splashActive && (keyShortClick(Key::Left) || keyLongPress(Key::Left, true)))
    {
        page = (uint8_t)(((page - 1 + totalPages - 1) % totalPages) + 1); // cycle 1..totalPages
        pageChanged = true;
    }

    // Arm SETTINGS when entering page 5; flush pending C
    if (!splashActive && page != prevPage)
    {
        if (page == totalPages)
        {
            (void)keyReleased(Key::Center);        // flush stale release
            settingsArmed = !keyDown(Key::Center); // arm only if not currently held
        }
        else
        {
            settingsArmed = false;
        }
        prevPage = page;
    }

    // If we arrived holding C, arm after it is released
    if (!splashActive && page == totalPages && !settingsArmed && !keyDown(Key::Center))
        settingsArmed = true;

    // DOWN release: quick return to page 1
    if (!splashActive && keyReleased(Key::Down) && page != 1)
    {
        page = 1;
        pageChanged = true;
    }

    // ENTER (CENTER) release on SETTINGS page -> enter loop_settings (regardless of hold time)
    if (!splashActive && page == totalPages && settingsArmed && keyReleased(Key::Center))
    {
        settingsArmed = false;
        settingsRequested = true;
    }

    // UI: refresh at ~6.7 Hz (150ms), but immediately if page changed
    if (!pageChanged && !everyMs(DISPLAY_UI_REFRESH_INTERVAL_MS, oledTick))
        return;

    char line0[21], line1[21], line2[21], line3[21];
    const char *footerLeftText = nullptr;
    line0[0] = line1[0] = line2[0] = line3[0] = '\0';

    if (splashActive)
    {
        snprintf(line1, sizeof(line1), "    RC CONTROLLER");
        snprintf(line2, sizeof(line2), "       by LUKE");
    }
    else
    {
        switch (page)
        {
        case 1:
        {
            static uint32_t battUiTick = 0;
            static uint16_t rxPctShown = 0;
            static uint16_t txPctShown = 0;
            const BatteryReading localBatt = batteryGetReading();
            const uint16_t rxPct = receiverGetBatteryPct();

            if (pageChanged || everyMs(DISPLAY_UI_REFRESH_INTERVAL_MS, battUiTick))
            {
                if (pageChanged || (abs((int)rxPct - (int)rxPctShown) >= 2))
                    rxPctShown = rxPct;
                if (pageChanged || (abs((int)localBatt.percent - (int)txPctShown) >= 2))
                    txPctShown = localBatt.percent;
            }

            snprintf(line0, sizeof(line0), "TX%3u%%      RX%3u%%", (unsigned)txPctShown, (unsigned)rxPctShown);
            line1[0] = '\0';
            snprintf(line2, sizeof(line1), "LX%+5d     RX%+5d", displayPct(joyL.readX()), displayPct(joyR.readX()));
            snprintf(line3, sizeof(line2), "LY%+5d     RY%+5d", displayPct(joyL.readY()), displayPct(joyR.readY()));
            break;
        }
        case 2:
            snprintf(line0, sizeof(line0), "XR %4d     YR %4d", joyL.readRawX(), joyL.readRawY());
            snprintf(line1, sizeof(line1), "XX%+5d     YY%+5d", displayPct(joyL.readLinearX()), displayPct(joyL.readLinearY()));
            snprintf(line2, sizeof(line2), "X%+6d     Y%+6d", displayPct(joyL.readX()), displayPct(joyL.readY()));
            line3[0] = '\0';
            footerLeftText = "LJ";
            break;

        case 3:
            snprintf(line0, sizeof(line0), "XR %4d     YR %4d", joyR.readRawX(), joyR.readRawY());
            snprintf(line1, sizeof(line1), "XX%+5d     YY%+5d", displayPct(joyR.readLinearX()), displayPct(joyR.readLinearY()));
            snprintf(line2, sizeof(line2), "X%+6d     Y%+6d", displayPct(joyR.readX()), displayPct(joyR.readY()));
            line3[0] = '\0';
            footerLeftText = "RJ";
            break;

        case 4:
            snprintf(line0, sizeof(line0), "PHOTO raw:%04d", photoSensorReadRaw());
            snprintf(line1, sizeof(line1), "PHOTO pct:%03u %%", (unsigned)photoSensorReadPct());
            line2[0] = '\0';
            line3[0] = '\0';
            break;

        case 5:
            snprintf(line0, sizeof(line0), " SETTINGS");
            snprintf(line2, sizeof(line2), " PRESS C TO ENTER");
            line1[0] = '\0';
            line3[0] = '\0';
            break;

        default:
            page = 1;
            prevPage = page;
            settingsArmed = false;
            pageChanged = true;
            break;
        }
    }

    uiRenderPage(line0,
                 line1,
                 line2,
                 line3,
                 true, // show footer with time/key when enabled
                 page,
                 totalPages,
                 buttonsLastReleaseKey(),
                 pageChanged,
                 footerLeftText);
}

bool screenMainConsumeSettingsRequest()
{
    if (settingsRequested)
    {
        settingsRequested = false;
        return true;
    }
    return false;
}
