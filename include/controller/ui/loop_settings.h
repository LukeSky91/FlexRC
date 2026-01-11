#pragma once
#include <stdint.h>

enum class LoopSettingsResult
{
    Stay = 0,
    StartCalibration,
    StartDeadband,
    StartExpo,
    StartKeyCalibration,
    ExitToMain
};

// Reset settings-loop state (e.g., when entering from loop_main).
void loopSettingsStart(uint8_t startPage = 1);

// Settings loop: navigation left/right, CENTER per page, DOWN per rules (see code).
LoopSettingsResult loopSettingsLoop(int mode, uint8_t batState);
