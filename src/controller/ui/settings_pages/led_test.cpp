#include <Arduino.h>
#include "controller/ui/settings_pages/led_test.h"
#include "controller/ui/menu.h"
#include "controller/ui/ui_input.h"
#include "controller/buttons.h"
#include "controller/leds.h"
#include "controller/config.h"
#include "common/time_utils.h"

namespace
{
constexpr uint8_t kLedCount = 3;
constexpr uint8_t kLedBrightnessPct = 35;

struct LedColorOption
{
    const char *name;
    Color color;
};

const LedColorOption kColorOptions[] = {
    {"OFF", OFF},
    {"RED", RED},
    {"GREEN", GREEN},
    {"BLUE", BLUE},
    {"YELLOW", YELLOW},
    {"MAGENTA", {255, 0, 255}},
    {"CYAN", {0, 255, 255}},
    {"WHITE", WHITE},
};

const LedSlot kLedSlots[kLedCount] = {
    LedSlot::First,
    LedSlot::Second,
    LedSlot::Third,
};

uint8_t selectedLed = 0;
uint8_t selectedColor[kLedCount] = {0, 0, 0};
uint32_t oledTick = 0;

void applyPreview()
{
    ledsManualOverrideBegin();
    for (uint8_t i = 0; i < kLedCount; ++i)
    {
        ledsManualOverrideSet(kLedSlots[i], kColorOptions[selectedColor[i]].color, kLedBrightnessPct);
    }
    ledsManualOverrideShow();
}

void render(bool forceRedraw)
{
    char line0[21], line1[21], line2[21], line3[21];

    for (uint8_t i = 0; i < kLedCount; ++i)
    {
        char *dst = (i == 0) ? line0 : (i == 1 ? line1 : line2);
        const char cursor = (i == selectedLed) ? '>' : ' ';
        snprintf(dst, 21, "%c L%u %s", cursor, (unsigned)(i + 1), kColorOptions[selectedColor[i]].name);
    }
    line3[0] = '\0';

    uiRenderPage(line0,
                 line1,
                 line2,
                 line3,
                 false,
                 4,
                 5,
                 buttonsLastReleaseKey(),
                 forceRedraw,
                 nullptr);
}
} // namespace

void ledTestStart()
{
    uiInputReset();
    oledTick = 0;
    selectedLed = 0;
    for (uint8_t i = 0; i < kLedCount; ++i)
        selectedColor[i] = 0;

    applyPreview();
    render(true);
}

LedTestResult ledTestLoop()
{
    const UiInputActions input = uiInputPoll();
    bool changed = false;

    if (input.selectNext)
    {
        selectedLed = (uint8_t)((selectedLed + 1) % kLedCount);
        render(true);
        return LedTestResult::Stay;
    }

    if (input.dec || input.decFast)
    {
        uint8_t &colorIdx = selectedColor[selectedLed];
        colorIdx = (colorIdx == 0) ? (uint8_t)(sizeof(kColorOptions) / sizeof(kColorOptions[0]) - 1) : (uint8_t)(colorIdx - 1);
        changed = true;
    }
    else if (input.inc || input.incFast)
    {
        uint8_t &colorIdx = selectedColor[selectedLed];
        colorIdx = (uint8_t)((colorIdx + 1) % (sizeof(kColorOptions) / sizeof(kColorOptions[0])));
        changed = true;
    }

    if (changed)
    {
        applyPreview();
        render(true);
        return LedTestResult::Stay;
    }

    if (input.back)
    {
        ledsManualOverrideEnd();
        return LedTestResult::ExitToSettings;
    }

    if (!everyMs(DISPLAY_UI_REFRESH_INTERVAL_MS, oledTick))
        return LedTestResult::Stay;

    render(false);
    return LedTestResult::Stay;
}
