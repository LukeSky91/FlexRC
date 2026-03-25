#pragma once

enum class IoReadingsResult
{
    Stay = 0,
    ExitToSettings
};

void ioReadingsStart();
IoReadingsResult ioReadingsLoop();
