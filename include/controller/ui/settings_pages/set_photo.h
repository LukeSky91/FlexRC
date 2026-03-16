#pragma once

enum class PhotoSettingsResult
{
    Stay = 0,
    ExitToSettings
};

void setPhotoStart();
PhotoSettingsResult setPhotoLoop();
