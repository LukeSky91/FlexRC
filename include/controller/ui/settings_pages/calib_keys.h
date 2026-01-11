#pragma once

enum class KeyCalibrationResult
{
    Running = 0,
    ExitToSettings
};

// Start kalibracji klawiatury (progi, live ADC)
void calibKeysStart();

// Keyboard calibration loop; returns ExitToSettings after exit.
KeyCalibrationResult calibKeysLoop();
