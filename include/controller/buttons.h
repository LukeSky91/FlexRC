#pragma once
#include <Arduino.h>

// Digital buttons wired directly to GPIOs.

enum class Key : uint8_t
{
    None = 0,
    Left,
    Right,
    Up,
    Down,
    Center,
    F1,
    F2,
    JL,
    JR
};

void buttonsTick(); // updates key state, does not consume events

void buttonsInit();

/*
 * Debounced state: is the given key held down.
 */
bool keyDown(Key k);

/*
 * EVENT 1: Short click
 * - fires on RELEASE
 * - only if press duration < thresholdMs
 * - suppressed if LONG already fired for that press
 */
bool keyShortClick(Key k, uint32_t thresholdMs = 800, bool consume = true);

/*
 * EVENT 2: Long press
 * - fires exactly after thresholdMs (no need to release)
 * - once per press
 * - if repeat=true, can fire again every repeatMs
 */
bool keyLongPress(Key k,
                  bool repeat = false,
                  uint32_t repeatMs = 300,
                  uint32_t thresholdMs = 800,
                  bool consume = true);

/*
 * EVENT 3: Release
 * - fires on RELEASE regardless of duration
 * - durationMs optional; pass pointer if you need press time
 */
bool keyReleased(Key k, uint32_t *durationMs = nullptr, bool consume = true);

// Last released key.
Key buttonsLastReleaseKey();

// Clear all pending events/holds (e.g., when entering a new screen)
void buttonsConsumeAll();
