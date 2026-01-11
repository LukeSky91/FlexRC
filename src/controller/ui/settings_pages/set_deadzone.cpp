#include <Arduino.h>
#include "controller/ui/settings_pages/set_deadzone.h"
#include "controller/ui/menu.h"
#include "controller/joysticks.h"
#include "controller/buttons.h"
#include "common/time_utils.h"

static uint32_t oledTick = 0;
static uint32_t saveUntilMs = 0;

enum class DzAxis : uint8_t
{
    LX = 0,
    LY,
    RX,
    RY,
    Count
};
static DzAxis selected = DzAxis::LX;
static int currentDz[4] = {0, 0, 0, 0};
static int originalDz[4] = {0, 0, 0, 0};

static const char *axisName(uint8_t idx)
{
    switch (idx)
    {
    case 0:
        return "LX";
    case 1:
        return "LY";
    case 2:
        return "RX";
    case 3:
        return "RY";
    default:
        return "--";
    }
}

static bool armUp = false;
static bool armDown = false;
static bool armCenter = false;

static void render(bool forceRedraw)
{
    char line0[21], line1[21], line2[21], line3[21];
    const char *names[4] = {axisName(0), axisName(1), axisName(2), axisName(3)};

    auto fmtLine = [&](uint8_t idx, char *dst)
    {
        const char sel = (selected == (DzAxis)idx) ? '>' : ' ';
        snprintf(dst, 21, "%s%c%3d   %3d", names[idx], sel, currentDz[idx], originalDz[idx]);
    };

    fmtLine(0, line0);
    fmtLine(1, line1);
    fmtLine(2, line2);
    fmtLine(3, line3);

    const char *footerSave = "SAVE";
    bool showSave = (saveUntilMs != 0) && (millis() < saveUntilMs);
    if (!showSave)
        saveUntilMs = 0;

    uiRenderPage(line0,
                 line1,
                 line2,
                 line3,
                 false,
                 2, 4,
                 buttonsLastReleaseDuration(),
                 buttonsLastReleaseKey(),
                 forceRedraw,
                 showSave ? footerSave : "");
}

void setDeadzoneStart()
{
    oledTick = 0;
    buttonsConsumeAll();
    (void)keyReleased(Key::Up);
    (void)keyReleased(Key::Down);
    (void)keyReleased(Key::Center);
    armUp = !keyDown(Key::Up);
    armDown = !keyDown(Key::Down);
    armCenter = !keyDown(Key::Center);
    for (uint8_t i = 0; i < 4; i++)
    {
        currentDz[i] = joysticksGetDeadzoneAxis(i);
        originalDz[i] = currentDz[i];
    }
    selected = DzAxis::LX;
    render(true);
}

DeadbandResult setDeadzoneLoop()
{
    bool changed = false;
    // auto-arm after release
    if (!armUp && !keyDown(Key::Up))
        armUp = true;
    if (!armDown && !keyDown(Key::Down))
        armDown = true;
    if (!armCenter && !keyDown(Key::Center))
        armCenter = true;

    // LEFT/RIGHT: short +/-1, long (repeat co 800 ms) +/-5 dla wybranej osi
    if (keyShortClick(Key::Right))
    {
        uint8_t idx = (uint8_t)selected;
        currentDz[idx] += 1;
        changed = true;
    }
    else if (keyShortClick(Key::Left))
    {
        uint8_t idx = (uint8_t)selected;
        currentDz[idx] -= 1;
        changed = true;
    }
    if (keyLongPress(Key::Right, true, 800, 800))
    {
        uint8_t idx = (uint8_t)selected;
        currentDz[idx] += 5;
        changed = true;
    }
    else if (keyLongPress(Key::Left, true, 800, 800))
    {
        uint8_t idx = (uint8_t)selected;
        currentDz[idx] -= 5;
        changed = true;
    }

    if (changed)
    {
        uint8_t idx = (uint8_t)selected;
        joysticksSetDeadzoneAxis(idx, currentDz[idx]);
        currentDz[idx] = joysticksGetDeadzoneAxis(idx); // po clamp
        render(true);
        return DeadbandResult::Stay;
    }

    if (armUp && keyReleased(Key::Up))
    {
        armUp = false;
        selected = (DzAxis)(((uint8_t)selected + 1) % (uint8_t)DzAxis::Count);
        render(true);
        return DeadbandResult::Stay;
    }

    if (armCenter && keyReleased(Key::Center))
    {
        armCenter = false;
        joysticksSaveDeadzone();
        for (uint8_t i = 0; i < 4; i++)
            originalDz[i] = joysticksGetDeadzoneAxis(i);
        saveUntilMs = millis() + 1200; // SAVE na ~1.2s
        render(true);
        return DeadbandResult::Stay;
    }

    if (armDown && keyReleased(Key::Down))
    {
        armDown = false;
        for (uint8_t i = 0; i < 4; i++)
            joysticksSetDeadzoneAxis(i, originalDz[i]);
        return DeadbandResult::ExitToSettings;
    }

    if (!everyMs(250, oledTick))
        return DeadbandResult::Stay;

    render(false);
    return DeadbandResult::Stay;
}
