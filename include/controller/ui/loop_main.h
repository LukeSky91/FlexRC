#pragma once
#include <stdint.h>

// Update main screen (OLED): mode, battery, axes.
void screenMainLoop(int mode, uint8_t batState);

// Flag: did the user request entry into loop_settings (on SETTINGS page)?
// Returns true and clears the flag.
bool screenMainConsumeSettingsRequest();

// Set start page and optionally skip splash.
void screenMainSetStartPage(uint8_t startPage, bool skipSplash);
