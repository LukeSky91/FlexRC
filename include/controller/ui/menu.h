#pragma once
#include <stdint.h>
#include "controller/buttons.h"

// Initialize menu state
void menuInit();

// Main menu loop; call from loop(). Returns true when in calibration mode.
bool menuLoop(int mode, uint8_t batState);

// Shared helper for rendering UI pages with page number.
// When showFooter=false, the footer line is blank.
void uiRenderPage(const char *line0,
                  const char *line1,
                  const char *line2,
                  const char *line3,
                  bool showFooter,
                  uint8_t page,
                  uint8_t totalPages,
                  uint32_t lastPressMs,
                  Key lastKey,
                  bool forceRedraw,
                  const char *footerOverride = nullptr);
