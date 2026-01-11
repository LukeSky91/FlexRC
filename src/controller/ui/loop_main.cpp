#include <Arduino.h>
#include "common/time_utils.h"
#include "controller/ui/loop_main.h"
#include "controller/joysticks.h"
#include "controller/leds.h"
#include "controller/receiver.h"
#include "controller/buttons.h"
#include "controller/ui/menu.h"

static uint32_t oledTick = 0;
static uint8_t page = 1; // 1=JOYS PCT, 2=L, 3=R, 4=AUX, 5=SETTINGS
static const uint8_t totalPages = 5;
static bool splashInit = false;
static bool splashActive = true;
static uint32_t splashUntilMs = 0;
static bool settingsRequested = false;
static bool settingsArmed = false;
static uint8_t prevPage = 1;

static int16_t rawToPct(int raw)
{
    long centered = (long)raw - 512;
    long scaled = centered * 100 / 511; // 0..1023 -> -100..100
    if (scaled > 100)
        scaled = 100;
    if (scaled < -100)
        scaled = -100;
    return (int16_t)scaled;
}

static int16_t mapToPct(int16_t v)
{
    long scaled = (long)v * 100 / 32767;
    if (scaled > 100)
        scaled = 100;
    if (scaled < -100)
        scaled = -100;
    return (int16_t)scaled;
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

    // LEDs: throttle show() calls
    static uint32_t ledTick = 0;
    if (everyMs(100, ledTick))
    {
        ledsSet(LedSlot::First, RED);
        ledsSet(LedSlot::Second, GREEN);
    }

    // UI: refresh at max 10 Hz (100ms), but immediately if page changed
    if (!pageChanged && !everyMs(250, oledTick))
        return;

    char line0[21], line1[21], line2[21], line3[21];
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
            snprintf(line0, sizeof(line0), "LX%+5d     RX%+5d", mapToPct(joyL.readX()), mapToPct(joyR.readX()));
            snprintf(line1, sizeof(line1), "LY%+5d     RY%+5d", mapToPct(joyL.readY()), mapToPct(joyR.readY()));
            line2[0] = '\0';
            line3[0] = '\0';
            break;

        case 2:
            snprintf(line0, sizeof(line0), "XR%+5d     YR%+5d", rawToPct(joyL.readRawX()), rawToPct(joyL.readRawY()));
            snprintf(line1, sizeof(line1), "XX%+5d     YY%+5d", mapToPct(joyL.readX()), mapToPct(joyL.readY()));
            snprintf(line2, sizeof(line2), "X%+6d     Y%+6d", joyL.readX(), joyL.readY());
            snprintf(line3, sizeof(line3), "THE LEFT JOY");
            break;

        case 3:
            snprintf(line0, sizeof(line0), "XR%+5d     YR%+5d", rawToPct(joyR.readRawX()), rawToPct(joyR.readRawY()));
            snprintf(line1, sizeof(line1), "XX%+5d     YY%+5d", mapToPct(joyR.readX()), mapToPct(joyR.readY()));
            snprintf(line2, sizeof(line2), "X%+6d     Y%+6d", joyR.readX(), joyR.readY());
            snprintf(line3, sizeof(line3), "THE RIGHT JOY");
            break;

        case 4:
        {
            static uint32_t auxUiTick = 0;
            static uint16_t auxUiShown = 0;

            if (pageChanged || everyMs(250, auxUiTick))
            {
                uint16_t auxRaw = receiverGetLastAux();

                if (pageChanged || (abs((int)auxRaw - (int)auxUiShown) >= 4))
                    auxUiShown = auxRaw;

                snprintf(line0, sizeof(line0), "AUX pct: %03u %%", (unsigned)auxUiShown);
                line1[0] = '\0';
                line2[0] = '\0';
                line3[0] = '\0';
            }
            else
            {
                return;
            }

            break;
        }

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
                 buttonsLastReleaseDuration(),
                 buttonsLastReleaseKey(),
                 pageChanged,
                 nullptr);
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
