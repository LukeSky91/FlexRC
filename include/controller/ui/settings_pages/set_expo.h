#pragma once

enum class ExpoResult
{
    Stay = 0,
    ExitToSettings
};

void setExpoStart();
ExpoResult setExpoLoop();
