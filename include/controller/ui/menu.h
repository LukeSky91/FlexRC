#pragma once
#include <stdint.h>
#include "controller/buttons.h"

// Initialize menu state
void menuInit();

// Main menu loop; call from loop(). Returns true when in calibration mode.
bool menuLoop(int mode, uint8_t batState);

// True only while the UI is inside screenMainLoop(), regardless of the current
// page number within that loop.
bool menuIsInMainLoop();

// Shared helper for rendering UI pages with page number.
// When showFooter=false, the footer line is blank.
void uiRenderPage(const char *line0,
                  const char *line1,
                  const char *line2,
                  const char *line3,
                  bool showFooter,
                  uint8_t page,
                  uint8_t totalPages,
                  Key lastKey,
                  bool forceRedraw,
                  const char *footerLeftText = nullptr);
