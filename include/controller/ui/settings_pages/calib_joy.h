#pragma once

enum class CalibrationResult
{
    Running = 0,     // calibration in progress
    Saved,           // save finished (return to loop_settings)
    ExitToMain       // user wants to return to loop_main
};

// Start joystick calibration mode (reset min/max, set up LED/LCD)
void calibJoyStart();

// Handle joystick calibration; returns a value from CalibrationResult.
CalibrationResult calibJoyLoop();
