#include <Arduino.h>
#include "controller/ui/menu.h"
#include "controller/buttons.h"
#include "controller/display.h"
#include "controller/ui/loop_main.h"
#include "controller/ui/loop_settings.h"
#include "controller/ui/settings_pages/calib_joy.h"
#include "controller/ui/settings_pages/calib_keys.h"
#include "controller/ui/settings_pages/set_deadzone.h"
#include "controller/ui/settings_pages/set_expo.h"
#include "controller/config.h"
#include "common/time_utils.h"

enum class UiMode
{
    Main = 0,
    Settings,
    JoyCalibration,
    KeyCalibration,
    Deadband,
    Expo
};

static UiMode uiMode = UiMode::Main;

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
    case StartScreen::DirectKeysThr:
        calibKeysStart();
        uiMode = UiMode::KeyCalibration;
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
        if (r == LoopSettingsResult::StartKeyCalibration)
        {
            calibKeysStart();
            uiMode = UiMode::KeyCalibration;
            return true; // block comm during keyboard calibration
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

    case UiMode::KeyCalibration:
    {
        KeyCalibrationResult kr = calibKeysLoop();
        if (kr == KeyCalibrationResult::ExitToSettings)
        {
            loopSettingsStart(4); // return to KEY THR page
            uiMode = UiMode::Settings;
            return false;
        }
        return true;
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
                  uint32_t lastPressMs,
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
    const bool footerDue = forceRedraw || everyMs(250, footerTick);

    if (footerOverride)
    {
        if (footerDue)
            displayText(4, footerOverride);
    }
    else if (showFooter)
    {
        if (footerDue)
        {
            char line4[21];

            uint32_t shown = lastPressMs;
            if (shown > 99999u)
                shown = 99999u;

            char keyChar = '-';
            switch (lastKey)
            {
            case Key::Left:
                keyChar = 'L';
                break;
            case Key::Right:
                keyChar = 'R';
                break;
            case Key::Up:
                keyChar = 'U';
                break;
            case Key::Down:
                keyChar = 'D';
                break;
            case Key::Center:
                keyChar = 'C';
                break;
            default:
                keyChar = '-';
                break;
            }

            if (FOOTER_TIMEKEY_ENABLE)
                snprintf(line4, sizeof(line4), "%5lu %c        [%u/%u]",
                         (unsigned long)shown, keyChar, page, totalPages);
            else
                snprintf(line4, sizeof(line4), "               [%u/%u]",
                         page, totalPages);

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
