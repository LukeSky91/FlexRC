#include <Arduino.h>
#include "controller/ui/settings_pages/io_readings.h"
#include "controller/ui/menu.h"
#include "controller/buttons.h"
#include "controller/config.h"
#include "common/time_utils.h"

namespace
{
uint32_t oledTick = 0;
uint8_t subPage = 1;
constexpr uint8_t kTotalPages = 5;

void render(bool forceRedraw)
{
    char line0[21], line1[21], line2[21], line3[21];
    const char *footerLeft = nullptr;

    switch (subPage)
    {
    case 1:
        snprintf(line0, sizeof(line0), "CH0 RY %4d", analogRead(HW_JOY_R_PIN_Y));
        snprintf(line1, sizeof(line1), "CH1 RX %4d", analogRead(HW_JOY_R_PIN_X));
        snprintf(line2, sizeof(line2), "CH3 LY %4d", analogRead(HW_JOY_L_PIN_Y));
        snprintf(line3, sizeof(line3), "CH4 LX %4d", analogRead(HW_JOY_L_PIN_X));
        footerLeft = "IO RAW";
        break;

    case 2:
        snprintf(line0, sizeof(line0), "CH7 PH %4d", analogRead(HW_PHOTO_PIN));
        snprintf(line1, sizeof(line1), "CH8 BT %4d", analogRead(HW_BATTERY_PIN));
        line2[0] = '\0';
        line3[0] = '\0';
        footerLeft = "IO RAW";
        break;

    case 3:
        snprintf(line0, sizeof(line0), "GP11 U  %1d", digitalRead(HW_BTN_UP_PIN) == LOW ? 1 : 0);
        snprintf(line1, sizeof(line1), "GP12 L  %1d", digitalRead(HW_BTN_LEFT_PIN) == LOW ? 1 : 0);
        snprintf(line2, sizeof(line2), "GP13 C  %1d", digitalRead(HW_BTN_CENTER_PIN) == LOW ? 1 : 0);
        snprintf(line3, sizeof(line3), "GP14 R  %1d", digitalRead(HW_BTN_RIGHT_PIN) == LOW ? 1 : 0);
        footerLeft = "IO RAW";
        break;

    case 4:
        snprintf(line0, sizeof(line0), "GP15 D  %1d", digitalRead(HW_BTN_DOWN_PIN) == LOW ? 1 : 0);
        snprintf(line1, sizeof(line1), "GP42 F1 %1d", digitalRead(HW_BTN_F1_PIN) == LOW ? 1 : 0);
        snprintf(line2, sizeof(line2), "GP41 F2 %1d", digitalRead(HW_BTN_F2_PIN) == LOW ? 1 : 0);
        snprintf(line3, sizeof(line3), "GP6  RB %1d", digitalRead(HW_JOY_R_PIN_BTN) == LOW ? 1 : 0);
        footerLeft = "IO RAW";
        break;

    case 5:
    default:
        snprintf(line0, sizeof(line0), "GP7  LB %1d", digitalRead(HW_JOY_L_PIN_BTN) == LOW ? 1 : 0);
        line1[0] = '\0';
        line2[0] = '\0';
        line3[0] = '\0';
        footerLeft = "IO RAW";
        break;
    }

    uiRenderPage(line0,
                 line1,
                 line2,
                 line3,
                 true,
                 subPage,
                 kTotalPages,
                 buttonsLastReleaseKey(),
                 forceRedraw,
                 footerLeft);
}
} // namespace

void ioReadingsStart()
{
    buttonsConsumeAll();
    (void)keyReleased(Key::Left);
    (void)keyReleased(Key::Right);
    (void)keyReleased(Key::Down);
    subPage = 1;
    oledTick = 0;
    render(true);
}

IoReadingsResult ioReadingsLoop()
{
    bool pageChanged = false;

    if (keyShortClick(Key::Right) || keyLongPress(Key::Right, true))
    {
        subPage = (uint8_t)(((subPage - 1 + 1) % kTotalPages) + 1);
        pageChanged = true;
    }
    else if (keyShortClick(Key::Left) || keyLongPress(Key::Left, true))
    {
        subPage = (uint8_t)(((subPage - 1 + kTotalPages - 1) % kTotalPages) + 1);
        pageChanged = true;
    }

    if (keyReleased(Key::Down))
        return IoReadingsResult::ExitToSettings;

    if (!pageChanged && !everyMs(DISPLAY_UI_REFRESH_INTERVAL_MS, oledTick))
        return IoReadingsResult::Stay;

    render(pageChanged);
    return IoReadingsResult::Stay;
}
