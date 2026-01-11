#pragma once

enum class DeadbandResult
{
    Stay = 0,
    ExitToSettings
};

void setDeadzoneStart();
DeadbandResult setDeadzoneLoop();
