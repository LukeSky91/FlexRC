#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "controller/config.h"
#include "controller/leds.h"

#define LED_COUNT 3

static Adafruit_NeoPixel strip(LED_COUNT, LED_RGB_PIN, NEO_GRB + NEO_KHZ800);

// Throttle show to avoid RX/UI stutter
static const uint32_t MIN_SHOW_INTERVAL_MS = 20; // 50 Hz (set to 30 for ~33 Hz)
static uint32_t lastShowMs = 0;
static bool dirty = false;
static bool manualOverrideActive = false;
static bool internalWrite = false;

static uint8_t slotIndex(LedSlot slot)
{
    switch (slot)
    {
    case LedSlot::First:
        return 0;
    case LedSlot::Second:
        return 1;
    case LedSlot::Third:
        return 2;
    }
    return 0;
}

static uint8_t applyBrightness(uint8_t value, uint8_t brightnessPct)
{
    uint8_t pct = (brightnessPct > 100) ? 100 : brightnessPct;
    return (uint8_t)((value * pct) / 100);
}

static uint32_t encodeColorForSlot(LedSlot slot, const Color &scaled)
{
    switch (slot)
    {
    case LedSlot::First:
    case LedSlot::Second:
        return strip.Color(scaled.g, scaled.r, scaled.b);
    case LedSlot::Third:
    default:
        return strip.Color(scaled.r, scaled.g, scaled.b);
    }
}

static void setPixel(LedSlot slot, Color c, uint8_t brightnessPct)
{
    Color scaled{
        applyBrightness(c.r, brightnessPct),
        applyBrightness(c.g, brightnessPct),
        applyBrightness(c.b, brightnessPct)};

    uint8_t idx = slotIndex(slot);
    uint32_t newColor = encodeColorForSlot(slot, scaled);

    if (strip.getPixelColor(idx) == newColor)
    {
        return;
    }

    strip.setPixelColor(idx, newColor);
    dirty = true;
}

void ledsInit()
{
    strip.begin();
    strip.clear();
    strip.show();
    lastShowMs = millis();
    dirty = false;
    manualOverrideActive = false;
    internalWrite = false;
}

void ledsSet(LedSlot slot, Color c, uint8_t brightnessPct)
{
    if (manualOverrideActive && !internalWrite)
    {
        return;
    }
    setPixel(slot, c, brightnessPct);
}

void ledsShow()
{
    if (!dirty)
        return;

    uint32_t now = millis();
    if (now - lastShowMs < MIN_SHOW_INTERVAL_MS)
    {
        return; // too soon
    }

    lastShowMs = now;
    strip.show();
    dirty = false;
}

void ledsAllOff()
{
    strip.clear();
    dirty = true;
    ledsShow(); // respects throttle
}

void ledsManualOverrideBegin()
{
    manualOverrideActive = true;
}

void ledsManualOverrideSet(LedSlot slot, Color c, uint8_t brightnessPct)
{
    manualOverrideActive = true;
    internalWrite = true;
    setPixel(slot, c, brightnessPct);
    internalWrite = false;
}

void ledsManualOverrideShow()
{
    manualOverrideActive = true;
    ledsShow();
}

void ledsManualOverrideEnd()
{
    internalWrite = true;
    strip.clear();
    dirty = true;
    manualOverrideActive = false;
    ledsShow();
    internalWrite = false;
}
