#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

void displayInit();
void displayText(int row, const char* txt);
void displayClear();

// Does not render immediately — only requests a flush.
// force=true -> render ASAP (bypasses the time limiter, still non-blocking).
void displayFlush(bool force = false);

// Must be called frequently (e.g., once per loop() iteration).
// Performs actual render and background OLED/I2C recovery.
void displayTick();

// Pixel overlay: drawn BEFORE text in renderAll().
// Pass nullptr, nullptr to disable.
typedef void (*DisplayOverlayFn)(U8G2 &oled, void *ctx);
void displaySetOverlay(DisplayOverlayFn fn, void *ctx);
