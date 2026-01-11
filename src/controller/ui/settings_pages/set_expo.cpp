#include <Arduino.h>
#include <math.h>
#include <U8g2lib.h>

#include "controller/ui/settings_pages/set_expo.h"
#include "controller/ui/menu.h"
#include "controller/joysticks.h"
#include "controller/buttons.h"
#include "common/time_utils.h"
#include "controller/display.h"

static uint32_t oledTick = 0;

static float currentExpo[4] = {0};
static float originalExpo[4] = {0};

static const uint8_t kMarkerCount = 11; // 11 markers = 10 segments
static bool armUp = false;
static bool armDown = false;
static bool armLeft = false;
static bool armRight = false;
static bool armCenter = false;

enum class ExpoItem : uint8_t
{
    Expo = 0,
    View,   // N/O
    Switch, // change axis
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
static void flushAndRearm()
{
    // 1) consume pending releases
    (void)keyReleased(Key::Up);
    (void)keyReleased(Key::Down);
    (void)keyReleased(Key::Left);
    (void)keyReleased(Key::Right);
    (void)keyReleased(Key::Center);

    // 2) consume pending short-clicks (IMPORTANT)
    (void)keyShortClick(Key::Up, 5000, true);
    (void)keyShortClick(Key::Down, 5000, true);
    (void)keyShortClick(Key::Left, 5000, true);
    (void)keyShortClick(Key::Right, 5000, true);
    (void)keyShortClick(Key::Center, 5000, true);

    // 3) re-arm
    armUp = !keyDown(Key::Up);
    armDown = !keyDown(Key::Down);
    armLeft = !keyDown(Key::Left);
    armRight = !keyDown(Key::Right);
    armCenter = !keyDown(Key::Center);
}

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

    float dzNorm = (float)joysticksGetDeadzoneAxis(axisIdx) / 512.0f;
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
    const char markNew = (selected == ExpoItem::Expo) ? '>' : ' ';

    int cur100 = (int)(currentExpo[axisIdx] * 100.0f + 0.5f);
    snprintf(buf, sizeof(buf), "%cexN:%d.%02d      ", markNew, cur100 / 100, cur100 % 100);
    oled.drawStr(0, 10, buf);

    int orig100 = (int)(originalExpo[axisIdx] * 100.0f + 0.5f);
    snprintf(buf, sizeof(buf), " ex :%d.%02d       ", orig100 / 100, orig100 % 100);
    oled.drawStr(0, 22, buf);

    oled.setFontMode(1); // restore transparent mode (optional)
}

static void render(bool forceRedraw)
{
    char line4[21];

    const char selView = (selected == ExpoItem::View) ? '>' : ' ';
    const char selSwitch = (selected == ExpoItem::Switch) ? '>' : ' ';
    const char viewChar = (viewMode == ViewMode::New) ? 'N' : 'O';
    const uint8_t pageIdx = axisIdx + 1;
    const bool showSave = millis() < saveUntilMs;

    displayText(0, "");
    displayText(1, "");
    displayText(2, "");
    displayText(3, "");

    if (showSave)
    {
        snprintf(line4, sizeof(line4), "%s %c%c %cP SAVE [%u/4]",
                 axisLabel(axisIdx), selView, viewChar, selSwitch, pageIdx);
    }
    else
    {
        snprintf(line4, sizeof(line4), "%s %c%c %cP      [%u/4]",
                 axisLabel(axisIdx), selView, viewChar, selSwitch, pageIdx);
    }

    displayText(4, line4);
    displayFlush(forceRedraw);
}

static void applyCurrentToHardware()
{
    for (uint8_t i = 0; i < 4; ++i)
    {
        joysticksSetExpoAxis(i, clampExpoLocal(currentExpo[i]));
        currentExpo[i] = joysticksGetExpoAxis(i);
    }
}

void setExpoStart()
{
    oledTick = 0;
    buttonsConsumeAll();

    flushAndRearm();

    for (uint8_t i = 0; i < 4; ++i)
    {
        originalExpo[i] = joysticksGetExpoAxis(i);
        currentExpo[i] = originalExpo[i];
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
    bool changed = false;
    bool viewChanged = false;
    bool axisChanged = false;
    // Consume releases upfront so no stale events carry over between selections
    bool centerReleased = keyReleased(Key::Center);
    bool upReleased = keyReleased(Key::Up);
    bool downReleased = keyReleased(Key::Down);
    // auto-arm after release
    if (!armUp && !keyDown(Key::Up))
        armUp = true;
    if (!armDown && !keyDown(Key::Down))
        armDown = true;
    if (!armLeft && !keyDown(Key::Left))
        armLeft = true;
    if (!armRight && !keyDown(Key::Right))
        armRight = true;
    if (!armCenter && !keyDown(Key::Center))
        armCenter = true;

    // Up: change selection
    if (armUp && upReleased)
    {
        armUp = false;
        selected = (ExpoItem)(((uint8_t)selected + 1) % (uint8_t)ExpoItem::Count);
        saveUntilMs = 0; // moving cursor cancels SAVE indicator
        flushAndRearm();
        render(true);
        return ExpoResult::Stay;
    }

    // Switch: change axis
    if (selected == ExpoItem::Switch)
    {
        bool leftReleased = keyReleased(Key::Left);
        bool rightReleased = keyReleased(Key::Right);
        if (armLeft && leftReleased)
        {
            armLeft = false;
            axisIdx = (axisIdx == 0) ? 3 : (axisIdx - 1);
            axisChanged = true;
            saveUntilMs = 0;
        }
        if (armRight && rightReleased)
        {
            armRight = false;
            axisIdx = (uint8_t)((axisIdx + 1) % 4);
            axisChanged = true;
            saveUntilMs = 0;
        }
    }

    if (selected == ExpoItem::Expo && armRight && keyShortClick(Key::Right))
    {
        armRight = false;
        currentExpo[axisIdx] += 0.01f;
        changed = true;
    }
    else if (selected == ExpoItem::Expo && armLeft && keyShortClick(Key::Left))
    {
        armLeft = false;
        currentExpo[axisIdx] -= 0.01f;
        changed = true;
    }

    if (selected == ExpoItem::Expo && keyLongPress(Key::Right, true, 800, 800))
    {
        currentExpo[axisIdx] += 0.05f;
        changed = true;
    }
    else if (selected == ExpoItem::Expo && keyLongPress(Key::Left, true, 800, 800))
    {
        currentExpo[axisIdx] -= 0.05f;
        changed = true;
    }

    // Toggle view N/O
    if (selected == ExpoItem::View)
    {
        bool leftReleased = keyReleased(Key::Left);
        bool rightReleased = keyReleased(Key::Right);
        if ((armRight && rightReleased) || (armLeft && leftReleased))
        {
            if (rightReleased)
                armRight = false;
            if (leftReleased)
                armLeft = false;
            viewMode = (viewMode == ViewMode::New) ? ViewMode::Old : ViewMode::New;
            viewChanged = true;
            saveUntilMs = 0;
        }
    }

    if (changed)
    {
        currentExpo[axisIdx] = clampExpoLocal(currentExpo[axisIdx]);
        viewMode = ViewMode::New;
        applyCurrentToHardware();
        render(true);
        return ExpoResult::Stay;
    }

    if (viewChanged || axisChanged)
    {
        flushAndRearm();
        render(true);
        return ExpoResult::Stay;
    }

    // Save only current axis when cursor is on Expo; stay on screen
    if (selected == ExpoItem::Expo && armCenter && centerReleased)
    {
        armCenter = false;
        applyCurrentToHardware();
        joysticksSaveExpoAxis(axisIdx);
        originalExpo[axisIdx] = currentExpo[axisIdx];
        saveUntilMs = millis() + 1200;
        render(true);
        return ExpoResult::Stay;
    }

    // Back without saving: restore values from entry
    if (armDown && downReleased)
    {
        armDown = false;
        for (uint8_t i = 0; i < 4; ++i)
            joysticksSetExpoAxis(i, originalExpo[i]);
        displaySetOverlay(nullptr, nullptr);
        return ExpoResult::ExitToSettings;
    }

    // idle render every ~50ms
    if (!everyMs(50, oledTick))
        return ExpoResult::Stay;

    render(false);
    return ExpoResult::Stay;
}
