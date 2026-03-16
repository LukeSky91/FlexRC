#pragma once

struct UiInputActions
{
    bool pagePrev = false;
    bool pageNext = false;
    bool selectNext = false;
    bool back = false;
    bool enter = false;
    bool dec = false;
    bool inc = false;
    bool decFast = false;
    bool incFast = false;
};

void uiInputReset();
UiInputActions uiInputPoll();
