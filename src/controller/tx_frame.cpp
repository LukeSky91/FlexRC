#include "controller/tx_frame.h"

#include "controller/buttons.h"
#include "controller/joysticks.h"

static int8_t controlToTx(float v)
{
    if (v > 100.0f)
        v = 100.0f;
    if (v < -100.0f)
        v = -100.0f;
    return (int8_t)((v >= 0.0f) ? (v + 0.5f) : (v - 0.5f));
}

CommFrame txFrameBuild(bool sendLiveControls)
{
    CommFrame tx{
        0,
        0,
        0,
        0,
        0u,
        0u};

    if (!sendLiveControls)
        return tx;

    tx.lx = controlToTx(joyL.readX());
    tx.ly = controlToTx(joyL.readY());
    tx.rx = controlToTx(joyR.readX());
    tx.ry = controlToTx(joyR.readY());
    if (keyDown(Key::JL))
        tx.joyButtons |= 0x01u;
    if (keyDown(Key::JR))
        tx.joyButtons |= 0x02u;
    return tx;
}
