#include <Arduino.h>
#include "controller/ui/loop_settings.h"
#include "controller/ui/menu.h"
#include "controller/config.h"
#include "controller/buttons.h"
#include "common/time_utils.h"

static uint32_t oledTick = 0;
// 1=CALIB JOYS, 2=JOYS EXPO, 3=LED TEST, 4=PHOTO, 5=IO READINGS
static uint8_t page = 1;
static const uint8_t totalPages = 5;
static bool initDone = false;
static uint8_t prevPage = 1;
static bool centerArmed = false;
static bool downArmed = false;

void loopSettingsStart(uint8_t startPage)
{
    buttonsConsumeAll();
    (void)keyReleased(Key::Center);
    (void)keyReleased(Key::Down);
    if (startPage < 1 || startPage > totalPages)
        startPage = 1;
    page = startPage;
    prevPage = page;
    centerArmed = false;
    downArmed = false;
    oledTick = 0;
    initDone = false;
}

LoopSettingsResult loopSettingsLoop(int mode, uint8_t batState)
{
    (void)mode;
    (void)batState;

    bool pageChanged = false;

    if (!initDone)
    {
        initDone = true;
        pageChanged = true; // first render with selected page
    }

    // LEFT/RIGHT: click or hold with auto-repeat (wrap)
    if (keyShortClick(Key::Right) || keyLongPress(Key::Right, true))
    {
        page = (uint8_t)(((page - 1 + 1) % totalPages) + 1); // cycle 1..N
        pageChanged = true;
    }
    else if (keyShortClick(Key::Left) || keyLongPress(Key::Left, true))
    {
        page = (uint8_t)(((page - 1 + totalPages - 1) % totalPages) + 1); // cycle 1..N
        pageChanged = true;
    }

    // Arm actions when page changes: flush stale releases
    if (page != prevPage)
    {
        (void)keyReleased(Key::Center); // flush
        (void)keyReleased(Key::Down);   // flush

        centerArmed = !keyDown(Key::Center);
        downArmed = !keyDown(Key::Down);

        prevPage = page;
    }

    // If we entered while holding, arm after release
    if (!centerArmed && !keyDown(Key::Center))
        centerArmed = true;
    if (!downArmed && !keyDown(Key::Down))
        downArmed = true;

    // DOWN release: on CALIBRATION -> exit to loop_main; otherwise jump to CALIBRATION
    if (downArmed && keyReleased(Key::Down))
    {
        if (page == 1)
        {
            return LoopSettingsResult::ExitToMain;
        }
        page = 1;
        pageChanged = true;
    }

    // ENTER (CENTER) release: action per page
    if (centerArmed && keyReleased(Key::Center))
    {
        if (page == 1)
        {
            return LoopSettingsResult::StartCalibration;
        }
        else if (page == 2)
        {
            return LoopSettingsResult::StartExpo;
        }
        else if (page == 3)
        {
            return LoopSettingsResult::StartLedTest;
        }
        else if (page == 4)
        {
            return LoopSettingsResult::StartPhotoSettings;
        }
        else if (page == 5)
        {
            return LoopSettingsResult::StartIoReadings;
        }
    }

    // UI limiter: max 10 Hz (100 ms), unless pageChanged
    if (!pageChanged && !everyMs(DISPLAY_UI_REFRESH_INTERVAL_MS, oledTick))
        return LoopSettingsResult::Stay;

    char line0[21], line1[21], line2[21], line3[21];
    line0[0] = line1[0] = line2[0] = line3[0] = '\0';

    switch (page)
    {
    case 1:
        snprintf(line0, sizeof(line0), "   JOYSTICKS");
        snprintf(line1, sizeof(line1), "   CALIBRATE");
        line2[0] = '\0';
        break;

    case 2:
        snprintf(line0, sizeof(line0), "   JOYSTICKS");
        snprintf(line1, sizeof(line1), "   EXPO");
        line2[0] = '\0';
        break;

    case 3:
        snprintf(line0, sizeof(line0), "   LED");
        snprintf(line1, sizeof(line1), "   TEST");
        line2[0] = '\0';
        break;

    case 4:
        snprintf(line0, sizeof(line0), "   PHOTO");
        snprintf(line1, sizeof(line1), "   SETTINGS");
        line2[0] = '\0';
        break;

    case 5:
        snprintf(line0, sizeof(line0), "   IO");
        snprintf(line1, sizeof(line1), "   READINGS");
        line2[0] = '\0';
        break;
    }

    uiRenderPage(line0, line1, line2, line3, true, page, totalPages, buttonsLastReleaseKey(), pageChanged, nullptr);
    return LoopSettingsResult::Stay;
}
