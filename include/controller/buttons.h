#pragma once
#include <Arduino.h>

/*
 * Analog keyboard on A7 (resistor ladder).
 *
 * Legacy naming:
 *  - LEFT  == old BTN1
 *  - RIGHT == old BTN2
 */

enum class Key : uint8_t
{
    None = 0,
    Left,
    Right,
    Up,
    Down,
    Center
};

void buttonsTick(); // updates key state, does not consume events

void buttonsInit();

/*
 * Current debounced key.
 */
Key buttonsCurrent();

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

// Last measured press duration (ms) at release for any key.
uint32_t buttonsLastReleaseDuration();

// Last released key.
Key buttonsLastReleaseKey();

// Current raw ADC reading (0..1023) of the keyboard.
uint16_t buttonsReadRawAdc();

// Per-key thresholds (pick the highest crossed threshold)
int buttonsGetThreshold(Key k);
void buttonsSetThreshold(Key k, int value);
void buttonsAdjustThreshold(Key k, int delta);
void buttonsSaveThresholds();

// Clear all pending events/holds (e.g., when entering a new screen)
void buttonsConsumeAll();
