#pragma once

#include <stdint.h>

void controlLinkInit();
void controlLinkTick(bool inMainLoop);
bool controlLinkAllowsLiveControls(bool inMainLoop);

