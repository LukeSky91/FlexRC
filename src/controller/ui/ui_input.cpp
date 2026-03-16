#include "controller/ui/ui_input.h"
#include "controller/buttons.h"

namespace
{
constexpr uint32_t kFastRepeatMs = 800;
constexpr uint32_t kFastThresholdMs = 800;
bool armed[(int)Key::JR + 1] = {};

void flushKey(Key key)
{
    (void)keyReleased(key);
    (void)keyShortClick(key, 5000, true);
}

void rearmAll()
{
    armed[(int)Key::Left] = !keyDown(Key::Left);
    armed[(int)Key::Right] = !keyDown(Key::Right);
    armed[(int)Key::Up] = !keyDown(Key::Up);
    armed[(int)Key::Down] = !keyDown(Key::Down);
    armed[(int)Key::Center] = !keyDown(Key::Center);
    armed[(int)Key::F1] = !keyDown(Key::F1);
    armed[(int)Key::F2] = !keyDown(Key::F2);
}

bool consumeRelease(Key key)
{
    const int idx = (int)key;
    if (!armed[idx] && !keyDown(key))
        armed[idx] = true;

    if (armed[idx] && keyReleased(key))
    {
        armed[idx] = false;
        return true;
    }

    return false;
}

bool consumeShort(Key key)
{
    const int idx = (int)key;
    if (!armed[idx] && !keyDown(key))
        armed[idx] = true;

    if (armed[idx] && keyShortClick(key))
    {
        armed[idx] = false;
        return true;
    }

    return false;
}
} // namespace

void uiInputReset()
{
    buttonsConsumeAll();
    flushKey(Key::Left);
    flushKey(Key::Right);
    flushKey(Key::Up);
    flushKey(Key::Down);
    flushKey(Key::Center);
    flushKey(Key::F1);
    flushKey(Key::F2);
    rearmAll();
}

UiInputActions uiInputPoll()
{
    UiInputActions actions{};

    actions.pagePrev = consumeRelease(Key::Left);
    actions.pageNext = consumeRelease(Key::Right);
    actions.selectNext = consumeRelease(Key::Up);
    actions.back = consumeRelease(Key::Down);
    actions.enter = consumeRelease(Key::Center);
    actions.dec = consumeShort(Key::F1);
    actions.inc = consumeShort(Key::F2);
    actions.decFast = keyLongPress(Key::F1, true, kFastRepeatMs, kFastThresholdMs);
    actions.incFast = keyLongPress(Key::F2, true, kFastRepeatMs, kFastThresholdMs);

    return actions;
}
