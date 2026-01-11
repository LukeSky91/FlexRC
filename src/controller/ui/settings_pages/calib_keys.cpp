#include <Arduino.h>
#include "controller/ui/settings_pages/calib_keys.h"
#include "controller/display.h"
#include "controller/buttons.h"
#include "common/time_utils.h"

// Order of navigation when pressing Up through items
enum class KeyCalibItem : uint8_t
{
    LiveAdc = 0,
    ThrUp,
    ThrLeft,
    ThrCenter,
    ThrRight,
    ThrDown,
    Back,
    Count
};

static KeyCalibItem selectedItem = KeyCalibItem::LiveAdc;
static uint32_t adcTick = 0;
static uint16_t lastAdc = 0;
static uint32_t oledTick = 0;
static uint8_t adcExitStep = 0; // progress of L->C->L sequence
static bool justEnteredAdc = false;
static uint32_t saveUntilMs = 0;

static bool isThresholdItem(KeyCalibItem it)
{
    return it == KeyCalibItem::ThrDown ||
           it == KeyCalibItem::ThrUp ||
           it == KeyCalibItem::ThrLeft ||
           it == KeyCalibItem::ThrCenter ||
           it == KeyCalibItem::ThrRight;
}

static Key itemToKey(KeyCalibItem it)
{
    switch (it)
    {
    case KeyCalibItem::ThrDown:
        return Key::Down;
    case KeyCalibItem::ThrUp:
        return Key::Up;
    case KeyCalibItem::ThrLeft:
        return Key::Left;
    case KeyCalibItem::ThrCenter:
        return Key::Center;
    case KeyCalibItem::ThrRight:
        return Key::Right;
    default:
        return Key::None;
    }
}

static void render(bool forceRedraw)
{
    auto mark = [](KeyCalibItem item, KeyCalibItem sel)
    { return (item == sel) ? '>' : ' '; };

    char line0[21], line1[21], line2[21], line3[21];

    // UP in the first row + sequence indicator only when on ADC
    if (selectedItem == KeyCalibItem::LiveAdc)
    {
        snprintf(line0, sizeof(line0), "         U%3d   %u/3",
                 buttonsGetThreshold(Key::Up), (unsigned)adcExitStep);
    }
    else
    {
        snprintf(line0, sizeof(line0), "        %cU%3d     ",
                 mark(KeyCalibItem::ThrUp, selectedItem), buttonsGetThreshold(Key::Up));
    }

    // L C R in the second row
    snprintf(line1, sizeof(line1), "%cL%3d   %cC%3d  %cR%3d",
             mark(KeyCalibItem::ThrLeft, selectedItem), buttonsGetThreshold(Key::Left),
             mark(KeyCalibItem::ThrCenter, selectedItem), buttonsGetThreshold(Key::Center),
             mark(KeyCalibItem::ThrRight, selectedItem), buttonsGetThreshold(Key::Right));

    // DOWN in the third row
    snprintf(line2, sizeof(line2), "        %cD%3d",
             mark(KeyCalibItem::ThrDown, selectedItem), buttonsGetThreshold(Key::Down));

    // Bottom line: ADC live readout
    if (selectedItem == KeyCalibItem::LiveAdc)
    {
        snprintf(line3, sizeof(line3), "%cADC     %04u   LCL^",
                 mark(KeyCalibItem::LiveAdc, selectedItem), (unsigned)lastAdc);
    }
    else
    {
        snprintf(line3, sizeof(line3), " ADC     ");
    }

    // Footer: center shows SAVE, right shows BACK selector
    char footer[21];
    memset(footer, ' ', sizeof(footer));
    footer[20] = '\0';
    // time + key info on the left
    uint32_t shown = buttonsLastReleaseDuration();
    if (shown > 99999u)
        shown = 99999u;
    char keyChar = '-';
    switch (buttonsLastReleaseKey())
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
    char timeBuf[11];
    snprintf(timeBuf, sizeof(timeBuf), " %05lums %c", (unsigned long)shown, keyChar);
    memcpy(footer, timeBuf, strlen(timeBuf));

    const bool showSave = millis() < saveUntilMs;
    if (showSave)
        memcpy(&footer[9], "SAVE", 4);
    const bool backSel = (selectedItem == KeyCalibItem::Back);
    const int backPos = 15;
    footer[backPos] = backSel ? '>' : ' ';
    memcpy(&footer[backPos + 1], "BACK", 4);

    displayText(0, line0);
    displayText(1, line1);
    displayText(2, line2);
    displayText(3, line3);
    displayText(4, footer);
    displayFlush(forceRedraw);
}

void calibKeysStart()
{
    selectedItem = KeyCalibItem::LiveAdc;
    adcTick = 0;
    oledTick = 0;
    lastAdc = buttonsReadRawAdc();
    adcExitStep = 0;
    justEnteredAdc = false;
    saveUntilMs = 0;
    render(true);
}

KeyCalibrationResult calibKeysLoop()
{
    bool changed = false;
    static KeyCalibItem prevItem = KeyCalibItem::LiveAdc;
    static bool backArmed = false;
    // Consume all releases up front to avoid stale events lingering
    bool relUp = keyReleased(Key::Up);
    // bool relDown = keyReleased(Key::Down);
    bool relLeft = keyReleased(Key::Left);
    bool relRight = keyReleased(Key::Right);
    bool relCenter = keyReleased(Key::Center);
    // Long-press repeat for threshold adjust (+/-5)
    bool longLeft = keyLongPress(Key::Left, true, 800);
    bool longRight = keyLongPress(Key::Right, true, 800);

    if (selectedItem != KeyCalibItem::LiveAdc && relUp)
    {
        selectedItem = static_cast<KeyCalibItem>((static_cast<uint8_t>(selectedItem) + 1) % (uint8_t)KeyCalibItem::Count);
        if (selectedItem == KeyCalibItem::LiveAdc)
        {
            adcExitStep = 0;
            justEnteredAdc = true;
        }
        else
        {
            justEnteredAdc = false;
        }
        changed = true;
    }

    if (isThresholdItem(selectedItem) && (relRight || longRight))
    {
        int delta = relRight ? 2 : 5;
        buttonsAdjustThreshold(itemToKey(selectedItem), delta);
        changed = true;
    }
    else if (isThresholdItem(selectedItem) && (relLeft || longLeft))
    {
        int delta = relLeft ? -2 : -5;
        buttonsAdjustThreshold(itemToKey(selectedItem), delta);
        changed = true;
    }

    // Special exit from ADC: sequence L -> C -> L (on release)
    if (selectedItem == KeyCalibItem::LiveAdc)
    {
        if (justEnteredAdc)
        {
            // discard any pending L/C releases when entering ADC
            relLeft = false;
            relCenter = false;
            adcExitStep = 0;
            justEnteredAdc = false;
        }

        if (adcExitStep == 0 && relLeft)
            adcExitStep = 1;
        else if (adcExitStep == 1 && relCenter)
            adcExitStep = 2;
        else if (adcExitStep == 2 && relLeft)
        {
            selectedItem = KeyCalibItem::ThrUp; // after sequence jump to UP
            adcExitStep = 0;
            changed = true;
        }
        else if (relLeft || relCenter)
        {
            adcExitStep = 0; // wrong order resets the sequence
        }
    }
    else
    {
        adcExitStep = 0; // reset progress when outside ADC
    }

    // Arm/back: flush pending Down when entering BACK, require next release to exit
    if (selectedItem != prevItem)
    {
        if (selectedItem == KeyCalibItem::Back)
        {
            (void)keyReleased(Key::Down); // flush any pending Down NOW
            backArmed = !keyDown(Key::Down);
        }
        else
        {
            backArmed = false;
        }
        prevItem = selectedItem;
    }

    if (selectedItem == KeyCalibItem::Back && backArmed && keyReleased(Key::Down))
    {
        backArmed = false;
        return KeyCalibrationResult::ExitToSettings;
    }

    // Save only selected threshold (C on that item); show SAVE
    if (isThresholdItem(selectedItem) && relCenter)
    {
        buttonsSaveThresholds();
        saveUntilMs = millis() + 1200;
        render(true);
        return KeyCalibrationResult::Running;
    }

    bool adcUpdated = false;
    if (selectedItem == KeyCalibItem::LiveAdc && everyMs(500, adcTick))
    {
        lastAdc = buttonsReadRawAdc();
        adcUpdated = true;
    }
    else if (selectedItem != KeyCalibItem::LiveAdc)
    {
        lastAdc = 0;
    }

    if (!changed && !adcUpdated && !everyMs(100, oledTick))
        return KeyCalibrationResult::Running;

    render(changed);
    return KeyCalibrationResult::Running;
}
