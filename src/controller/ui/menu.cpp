#include <Arduino.h>
#include "controller/ui/menu.h"
#include "controller/buttons.h"
#include "controller/display.h"
#include "controller/ui/loop_main.h"
#include "controller/ui/loop_settings.h"
#include "controller/ui/settings_pages/calib_joy.h"
#include "controller/ui/settings_pages/set_deadzone.h"
#include "controller/ui/settings_pages/set_expo.h"
#include "controller/ui/settings_pages/led_test.h"
#include "controller/ui/settings_pages/set_photo.h"
#include "controller/config.h"
#include "common/time_utils.h"

enum class UiMode
{
    Main = 0,
    Settings,
    JoyCalibration,
    Deadband,
    Expo,
    LedTest,
    PhotoSettings
};

static UiMode uiMode = UiMode::Main;

static void formatFooterWithPage(char *dst,
                                 size_t dstSize,
                                 const char *leftText,
                                 uint8_t page,
                                 uint8_t totalPages)
{
    if (!dst || dstSize == 0)
        return;

    char pageBuf[8];
    snprintf(pageBuf, sizeof(pageBuf), "[%u/%u]", page, totalPages);

    const size_t pageLen = strlen(pageBuf);
    const size_t width = (dstSize > 0) ? (dstSize - 1) : 0;
    const size_t leftWidth = (width > pageLen) ? (width - pageLen) : 0;

    char leftBuf[21];
    snprintf(leftBuf, sizeof(leftBuf), "%s", leftText ? leftText : "");
    snprintf(dst, dstSize, "%-*.*s%s",
             (int)leftWidth,
             (int)leftWidth,
             leftBuf,
             pageBuf);
}

void menuInit()
{
    uiMode = UiMode::Main;

    // Choose startup screen according to config.
    switch (START_SCREEN)
    {
    case StartScreen::DefaultSplash:
        // standard: splash + loop_main starting at page 1
        screenMainSetStartPage(1, false);
        break;
    case StartScreen::DirectMain:
        // skip splash, go straight to loop_main page 1
        screenMainSetStartPage(1, true);
        break;
    case StartScreen::DirectSetExpo:
        // jump directly into EXPO; navigation still works normally
        setExpoStart();
        uiMode = UiMode::Expo;
        break;
    case StartScreen::DirectCalibJoy:
        calibJoyStart();
        uiMode = UiMode::JoyCalibration;
        break;
    case StartScreen::DirectDeadzone:
        setDeadzoneStart();
        uiMode = UiMode::Deadband;
        break;
    default:
        screenMainSetStartPage(1, false);
        break;
    }
}

bool menuLoop(int mode, uint8_t batState)
{
    switch (uiMode)
    {
    case UiMode::Main:
        screenMainLoop(mode, batState);
        if (screenMainConsumeSettingsRequest())
        {
            loopSettingsStart();
            uiMode = UiMode::Settings;
        }
        return false; // do not block comm logic

    case UiMode::Settings:
    {
        LoopSettingsResult r = loopSettingsLoop(mode, batState);
        if (r == LoopSettingsResult::StartCalibration)
        {
            calibJoyStart();
            uiMode = UiMode::JoyCalibration;
            return true; // block comm during joystick calibration
        }
        if (r == LoopSettingsResult::StartDeadband)
        {
            setDeadzoneStart();
            uiMode = UiMode::Deadband;
            return true; // block comm during deadband page
        }
        if (r == LoopSettingsResult::StartExpo)
        {
            setExpoStart();
            uiMode = UiMode::Expo;
            return true; // block comm during expo page
        }
        if (r == LoopSettingsResult::StartLedTest)
        {
            ledTestStart();
            uiMode = UiMode::LedTest;
            return false;
        }
        if (r == LoopSettingsResult::StartPhotoSettings)
        {
            setPhotoStart();
            uiMode = UiMode::PhotoSettings;
            return false;
        }
        if (r == LoopSettingsResult::ExitToMain)
        {
            uiMode = UiMode::Main;
        }
        return false;
    }

    case UiMode::JoyCalibration:
    {
        CalibrationResult cr = calibJoyLoop();
        if (cr == CalibrationResult::ExitToMain)
        {
            // Return to settings (page 1), not to main screen
            loopSettingsStart(1);
            uiMode = UiMode::Settings;
            return false;
        }
        if (cr == CalibrationResult::Saved)
        {
            loopSettingsStart(); // back to settings, page 1
            uiMode = UiMode::Settings;
            return false;
        }
        return true; // Running
    }

    case UiMode::Deadband:
    {
        DeadbandResult dr = setDeadzoneLoop();
        if (dr == DeadbandResult::ExitToSettings)
        {
            loopSettingsStart(2); // return to deadzone page
            uiMode = UiMode::Settings;
            return false;
        }
        return true;
    }

    case UiMode::Expo:
    {
        ExpoResult er = setExpoLoop();
        if (er == ExpoResult::ExitToSettings)
        {
            loopSettingsStart(3); // return to EXPO page
            uiMode = UiMode::Settings;
            return false;
        }
        return true;
    }

    case UiMode::LedTest:
    {
        LedTestResult lr = ledTestLoop();
        if (lr == LedTestResult::ExitToSettings)
        {
            loopSettingsStart(4);
            uiMode = UiMode::Settings;
        }
        return false;
    }

    case UiMode::PhotoSettings:
    {
        PhotoSettingsResult pr = setPhotoLoop();
        if (pr == PhotoSettingsResult::ExitToSettings)
        {
            loopSettingsStart(5);
            uiMode = UiMode::Settings;
        }
        return false;
    }
    }

    return false;
}

void uiRenderPage(const char *line0,
                  const char *line1,
                  const char *line2,
                  const char *line3,
                  bool showFooter,
                  uint8_t page,
                  uint8_t totalPages,
                  Key lastKey,
                  bool forceRedraw,
                  const char *footerOverride)
{
    // 0..3: normalnie jak było
    displayText(0, line0 ? line0 : "");
    displayText(1, line1 ? line1 : "");
    displayText(2, line2 ? line2 : "");
    displayText(3, line3 ? line3 : "");

    // ===== FOOTER THROTTLE (punkt B) =====
    // Footer może pokazywać ms, ale nie może brudzić OLED częściej niż co 250ms.
    static uint32_t footerTick = 0;
    const bool footerDue = forceRedraw || everyMs(DISPLAY_UI_REFRESH_INTERVAL_MS, footerTick);

    if (footerOverride)
    {
        if (footerDue)
        {
            displayText(4, footerOverride);
        }
    }
    else if (showFooter)
    {
        if (footerDue)
        {
            char line4[21];
            const char *keyLabel = "--";
            switch (lastKey)
            {
            case Key::Left:
                keyLabel = "L";
                break;
            case Key::Right:
                keyLabel = "R";
                break;
            case Key::Up:
                keyLabel = "U";
                break;
            case Key::Down:
                keyLabel = "D";
                break;
            case Key::Center:
                keyLabel = "C";
                break;
            case Key::F1:
                keyLabel = "F1";
                break;
            case Key::F2:
                keyLabel = "F2";
                break;
            case Key::JL:
                keyLabel = "LJ";
                break;
            case Key::JR:
                keyLabel = "RJ";
                break;
            default:
                keyLabel = "--";
                break;
            }

            if (FOOTER_TIMEKEY_ENABLE)
                formatFooterWithPage(line4, sizeof(line4), keyLabel, page, totalPages);
            else
                formatFooterWithPage(line4, sizeof(line4), "", page, totalPages);

            displayText(4, line4);
        }
    }
    else
    {
        if (forceRedraw)
            displayText(4, "");
    }

    displayFlush(forceRedraw);
}
