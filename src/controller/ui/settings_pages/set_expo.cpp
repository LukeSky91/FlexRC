#include <Arduino.h>
#include <math.h>
#include <U8g2lib.h>

#include "controller/ui/settings_pages/set_expo.h"
#include "controller/ui/menu.h"
#include "controller/ui/ui_input.h"
#include "controller/joysticks.h"
#include "controller/buttons.h"
#include "common/time_utils.h"
#include "controller/display.h"
#include "controller/config.h"

static uint32_t oledTick = 0;

static float currentExpo[4] = {0};
static float originalExpo[4] = {0};
static int currentDeadzone[4] = {0};
static int originalDeadzone[4] = {0};

static const uint8_t kMarkerCount = 11; // 11 markers = 10 segments

enum class ExpoItem : uint8_t
{
    Expo = 0,
    Deadzone,
    View,
    Count
};

enum class ViewMode : uint8_t
{
    New = 0,
    Old
};

static ExpoItem selected = ExpoItem::Expo;
static ViewMode viewMode = ViewMode::New;
static uint8_t axisIdx = 0;
static uint32_t saveUntilMs = 0;

// cache (keep it: compute yCache only when the curve changes)
static bool curveValid = false;
static float lastExpo = -999.0f;
static float lastDz = -1.0f;
static uint8_t lastAxis = 255;
static ViewMode lastView = (ViewMode)255;
static uint8_t yCache[128];

static void render(bool forceRedraw);
static void applyCurrentToHardware();

static float clampExpoLocal(float e)
{
    if (e < 0.0f)
        e = 0.0f;
    if (e > 3.0f)
        e = 3.0f;
    return e;
}

static const char *axisLabel(uint8_t idx)
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
        return "??";
    }
}

static void overlayExpo(U8G2 &oled, void *)
{
    const int W = 128;
    const int topPad = 0;
    const int bottomPad = 12;
    const int H = 64 - topPad - bottomPad;

    auto pctFor = [](float expo, float xNorm, float deadzoneNorm) -> float
    {
        if (xNorm < deadzoneNorm)
            return 0.0f;
        float norm = (deadzoneNorm >= 0.999f) ? 0.0f : (xNorm - deadzoneNorm) / (1.0f - deadzoneNorm);
        norm = constrain(norm, 0.0f, 1.0f);
        float curved = pow(norm, 1.0f + expo);
        curved = constrain(curved, 0.0f, 1.0f);
        return curved * 100.0f;
    };

    float dzNorm = (float)currentDeadzone[axisIdx] / (float)ADC_CENTER;
    dzNorm = constrain(dzNorm, 0.0f, 0.999f);

    auto yFromPct = [&](float pct) -> int
    {
        pct = constrain(pct, 0.0f, 100.0f);
        int y = (int)lroundf((100.0f - pct) * (float)(H - 1) / 100.0f);
        y += topPad;
        if (y < topPad)
            y = topPad;
        if (y >= topPad + H)
            y = topPad + H - 1;
        return y;
    };

    const float expoShown = (viewMode == ViewMode::New) ? currentExpo[axisIdx] : originalExpo[axisIdx];
    const float dzShown = (viewMode == ViewMode::New) ? (float)currentDeadzone[axisIdx] : (float)originalDeadzone[axisIdx];
    dzNorm = dzShown / (float)ADC_CENTER;
    dzNorm = constrain(dzNorm, 0.0f, 0.999f);

    const float EXPO_EPS = 0.0005f;
    const float DZ_EPS = 0.0005f;

    const bool curveDirty =
        (!curveValid) ||
        (fabsf(expoShown - lastExpo) > EXPO_EPS) ||
        (fabsf(dzNorm - lastDz) > DZ_EPS) ||
        (axisIdx != lastAxis) ||
        (viewMode != lastView);

    // 1) Always clear the plot area to avoid artifacts
    oled.setDrawColor(0);
    oled.drawBox(0, topPad, W, H);
    oled.setDrawColor(1);

    // 2) Recompute cache only when needed
    if (curveDirty)
    {
        for (int x = 0; x < W; ++x)
        {
            float xNorm = (float)x / (float)(W - 1);
            int y = yFromPct(pctFor(expoShown, xNorm, dzNorm));
            yCache[x] = (uint8_t)constrain(y, 0, 63);
        }

        lastExpo = expoShown;
        lastDz = dzNorm;
        lastAxis = axisIdx;
        lastView = viewMode;
        curveValid = true;
    }

    // 3) Always draw the curve (using cache)
    for (int x = 1; x < W; ++x)
        oled.drawLine(x - 1, yCache[x - 1], x, yCache[x]);

    // 4) Always draw markers (using cache)
    for (int i = 0; i < (int)kMarkerCount; ++i)
    {
        int x = (kMarkerCount == 1) ? 0 : (int)lroundf((float)i * (float)(W - 1) / (float)(kMarkerCount - 1));
        x = constrain(x, 0, W - 1);

        int y = yCache[x];
        int y0 = (y > topPad + 1) ? y - 2 : topPad;
        int y1 = (y < topPad + H - 2) ? y + 2 : (topPad + H - 1);
        oled.drawLine(x, y0, x, y1);
    }

    // 5) Text in top-left; no panel, but solid glyph background clears old text
    oled.setFontMode(0); // solid background for glyphs
    oled.setDrawColor(1);

    char buf[28];
    const char markExpo = (selected == ExpoItem::Expo) ? '>' : ' ';
    const char markDz = (selected == ExpoItem::Deadzone) ? '>' : ' ';

    int cur100 = (int)(currentExpo[axisIdx] * 100.0f + 0.5f);
    int orig100 = (int)(originalExpo[axisIdx] * 100.0f + 0.5f);
    snprintf(buf, sizeof(buf), "%cex: %1d.%02d/%1d.%02d ",
             markExpo,
             cur100 / 100,
             cur100 % 100,
             orig100 / 100,
             orig100 % 100);
    oled.drawStr(0, 10, buf);

    snprintf(buf, sizeof(buf), "%cdzn:%4d/%-4d", markDz, currentDeadzone[axisIdx], originalDeadzone[axisIdx]);
    oled.drawStr(0, 22, buf);

    oled.setFontMode(1); // restore transparent mode (optional)
}

static void render(bool forceRedraw)
{
    char line4[21];
    char footerLeft[14];
    char footerRight[8];

    const char selView = (selected == ExpoItem::View) ? '>' : ' ';
    const char viewChar = (viewMode == ViewMode::New) ? 'N' : 'O';
    const uint8_t pageIdx = axisIdx + 1;
    const bool showSave = millis() < saveUntilMs;

    displayText(0, "");
    displayText(1, "");
    displayText(2, "");
    displayText(3, "");

    if (showSave)
    {
        snprintf(footerLeft, sizeof(footerLeft), "%s",
                 (String(axisLabel(axisIdx)) + " " + selView + viewChar + " SAVE").c_str());
    }
    else
    {
        snprintf(footerLeft, sizeof(footerLeft), "%s",
                 (String(axisLabel(axisIdx)) + " " + selView + viewChar).c_str());
    }

    snprintf(footerRight, sizeof(footerRight), "[%u/4]", pageIdx);
    snprintf(line4, sizeof(line4), "%-13.13s%7s", footerLeft, footerRight);

    displayText(4, line4);
    displayFlush(forceRedraw);
}

static void applyCurrentToHardware()
{
    for (uint8_t i = 0; i < 4; ++i)
    {
        joysticksSetExpoAxis(i, clampExpoLocal(currentExpo[i]));
        currentExpo[i] = joysticksGetExpoAxis(i);
        joysticksSetDeadzoneAxis(i, currentDeadzone[i]);
        currentDeadzone[i] = joysticksGetDeadzoneAxis(i);
    }
}

void setExpoStart()
{
    oledTick = 0;
    uiInputReset();

    for (uint8_t i = 0; i < 4; ++i)
    {
        originalExpo[i] = joysticksGetExpoAxis(i);
        currentExpo[i] = originalExpo[i];
        originalDeadzone[i] = joysticksGetDeadzoneAxis(i);
        currentDeadzone[i] = originalDeadzone[i];
    }

    // reset cache so first render always recomputes
    curveValid = false;
    lastAxis = 255;
    lastView = (ViewMode)255;
    lastExpo = -999.0f;
    lastDz = -1.0f;

    selected = ExpoItem::Expo;
    viewMode = ViewMode::New;
    axisIdx = 0;

    displaySetOverlay(overlayExpo, nullptr);

    render(true);
}

ExpoResult setExpoLoop()
{
    const UiInputActions input = uiInputPoll();
    bool changed = false;
    bool axisChanged = false;

    if (input.selectNext)
    {
        selected = (ExpoItem)(((uint8_t)selected + 1) % (uint8_t)ExpoItem::Count);
        saveUntilMs = 0;
        render(true);
        return ExpoResult::Stay;
    }

    if (input.pagePrev)
    {
        axisIdx = (axisIdx == 0) ? 3 : (axisIdx - 1);
        axisChanged = true;
        saveUntilMs = 0;
    }
    if (input.pageNext)
    {
        axisIdx = (uint8_t)((axisIdx + 1) % 4);
        axisChanged = true;
        saveUntilMs = 0;
    }

    if (selected == ExpoItem::Expo && input.inc)
    {
        currentExpo[axisIdx] += 0.01f;
        changed = true;
    }
    else if (selected == ExpoItem::Expo && input.dec)
    {
        currentExpo[axisIdx] -= 0.01f;
        changed = true;
    }

    if (selected == ExpoItem::Expo && input.incFast)
    {
        currentExpo[axisIdx] += 0.05f;
        changed = true;
    }
    else if (selected == ExpoItem::Expo && input.decFast)
    {
        currentExpo[axisIdx] -= 0.05f;
        changed = true;
    }

    if (selected == ExpoItem::Deadzone && input.inc)
    {
        currentDeadzone[axisIdx] += 1;
        changed = true;
    }
    else if (selected == ExpoItem::Deadzone && input.dec)
    {
        currentDeadzone[axisIdx] -= 1;
        changed = true;
    }
    else if (selected == ExpoItem::Deadzone && input.incFast)
    {
        currentDeadzone[axisIdx] += 5;
        changed = true;
    }
    else if (selected == ExpoItem::Deadzone && input.decFast)
    {
        currentDeadzone[axisIdx] -= 5;
        changed = true;
    }

    if (selected == ExpoItem::View && (input.inc || input.dec || input.incFast || input.decFast))
    {
        viewMode = (viewMode == ViewMode::New) ? ViewMode::Old : ViewMode::New;
        render(true);
        return ExpoResult::Stay;
    }

    if (changed)
    {
        currentExpo[axisIdx] = clampExpoLocal(currentExpo[axisIdx]);
        applyCurrentToHardware();
        render(true);
        return ExpoResult::Stay;
    }

    if (axisChanged)
    {
        render(true);
        return ExpoResult::Stay;
    }

    if (input.enter)
    {
        applyCurrentToHardware();
        joysticksSaveExpoAxis(axisIdx);
        joysticksSaveDeadzone();
        originalExpo[axisIdx] = currentExpo[axisIdx];
        originalDeadzone[axisIdx] = currentDeadzone[axisIdx];
        viewMode = ViewMode::New;
        saveUntilMs = millis() + 1200;
        render(true);
        return ExpoResult::Stay;
    }

    // Back without saving: restore values from entry
    if (input.back)
    {
        for (uint8_t i = 0; i < 4; ++i)
        {
            joysticksSetExpoAxis(i, originalExpo[i]);
            joysticksSetDeadzoneAxis(i, originalDeadzone[i]);
        }
        displaySetOverlay(nullptr, nullptr);
        return ExpoResult::ExitToSettings;
    }

    // idle render every ~50ms
    if (!everyMs(DISPLAY_UI_REFRESH_INTERVAL_MS, oledTick))
        return ExpoResult::Stay;

    render(false);
    return ExpoResult::Stay;
}
