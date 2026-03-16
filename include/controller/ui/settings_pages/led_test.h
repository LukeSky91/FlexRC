#pragma once

enum class LedTestResult
{
    Stay = 0,
    ExitToSettings
};

void ledTestStart();
LedTestResult ledTestLoop();
