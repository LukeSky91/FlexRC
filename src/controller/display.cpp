#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <string.h>
#include "controller/display.h"
#include "controller/config.h"

// SH1106 128x64 OLED via I2C, U8g2 page buffer.
U8G2_SH1106_128X64_NONAME_1_HW_I2C oled(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

static const uint8_t kRows = 5;
static const uint8_t kCols = 20;
static const uint8_t kLineHeight = 12; // for 6x10 font
static char lines[kRows][kCols + 1];

// ============ Buffer state ============
static bool dirty = false;

// ============ OLED limiter ============
static const uint32_t MIN_FLUSH_INTERVAL_MS = DISPLAY_MIN_FLUSH_INTERVAL_MS;
static uint32_t lastFlushMs = 0;

// ============ Async render ============
static bool flushRequested = false;
static bool flushForceRequested = false;
static DisplayOverlayFn overlayFn = nullptr;
static void *overlayCtx = nullptr;

// ============ Fault & recovery ============
static bool oledFault = false;
static uint32_t nextRecoverMs = 0;
static uint8_t recoverAttempts = 0;

// Recovery backoff (avoid hammering the bus)
static const uint32_t RECOVER_BACKOFF_MS = 800;
static const uint8_t RECOVER_MAX_ATTEMPTS_BEFORE_LONG_PAUSE = 5;
static const uint32_t RECOVER_LONG_PAUSE_MS = 5000;

// ======= Helpers =======
static void clearLines()
{
    for (uint8_t row = 0; row < kRows; ++row)
    {
        memset(lines[row], ' ', kCols);
        lines[row][kCols] = '\0';
    }
    dirty = true;
}

static void renderAll()
{
    oled.firstPage();
    do
    {
        for (uint8_t row = 0; row < kRows; ++row)
        {
            oled.drawStr(0, (row + 1) * kLineHeight, lines[row]);
        }
        // Draw overlay last so text (spaces) does not overwrite overlay lines.
        if (overlayFn)
            overlayFn(oled, overlayCtx);
    } while (oled.nextPage());

    dirty = false;
}

// Try to detect Wire timeout flag (AVR usually has it).
static bool i2cTimeoutFlagGet()
{
#if defined(ARDUINO_ARCH_AVR)
    return Wire.getWireTimeoutFlag();
#else
    return false;
#endif
}

static void i2cTimeoutFlagClear()
{
#if defined(ARDUINO_ARCH_AVR)
    Wire.clearWireTimeoutFlag();
#endif
}

// ---- I2C "unstick" ----
static uint8_t i2cPinSDA()
{
#if defined(ARDUINO_AVR_MEGA2560) || defined(ARDUINO_AVR_MEGA)
    return 20;
#else
    return A4;
#endif
}
static uint8_t i2cPinSCL()
{
#if defined(ARDUINO_AVR_MEGA2560) || defined(ARDUINO_AVR_MEGA)
    return 21;
#else
    return A5;
#endif
}

// Free the bus: 9 SCL clocks + STOP
static void i2cUnstick()
{
    const uint8_t sda = i2cPinSDA();
    const uint8_t scl = i2cPinSCL();

    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, INPUT_PULLUP);
    delayMicroseconds(5);

    for (uint8_t i = 0; i < 9; i++)
    {
        pinMode(scl, OUTPUT);
        digitalWrite(scl, LOW);
        delayMicroseconds(5);
        pinMode(scl, INPUT_PULLUP);
        delayMicroseconds(5);
    }

    pinMode(sda, OUTPUT);
    digitalWrite(sda, LOW);
    delayMicroseconds(5);

    pinMode(scl, INPUT_PULLUP);
    delayMicroseconds(5);

    pinMode(sda, INPUT_PULLUP);
    delayMicroseconds(5);
}

// Re-init I2C + OLED
static bool oledRecoverNow()
{
    i2cUnstick();

    Wire.end();
    delay(2);
    Wire.begin();
    Wire.setClock(400000);

    oled.begin();
    oled.setBusClock(400000);
    oled.setFont(u8g2_font_6x10_mr);

    dirty = true;
    flushRequested = true;
    flushForceRequested = true;

    return true;
}

void displayInit()
{
    oled.begin();
    oled.setBusClock(400000);
    oled.setFont(u8g2_font_6x10_mr);

    clearLines();

    flushRequested = true;
    flushForceRequested = true;
    lastFlushMs = millis();

    oledFault = false;
    nextRecoverMs = 0;
    recoverAttempts = 0;
}

void displayClear()
{
    clearLines();
    // only request a flush — rendering is done in displayTick()
    flushRequested = true;
}

void displayText(int row, const char *txt)
{
    if (row < 0 || row >= kRows || txt == nullptr)
        return;

    char newLine[kCols + 1];
    size_t len = strlen(txt);
    for (uint8_t i = 0; i < kCols; ++i)
        newLine[i] = (i < len) ? txt[i] : ' ';
    newLine[kCols] = '\0';

    if (memcmp(lines[row], newLine, kCols + 1) == 0)
        return;

    memcpy(lines[row], newLine, kCols + 1);
    dirty = true;
    flushRequested = true;
}

void displaySetOverlay(DisplayOverlayFn fn, void *ctx)
{
    overlayFn = fn;
    overlayCtx = ctx;
    // force redraw because overlay can change independently from text
    flushRequested = true;
    flushForceRequested = true;
    dirty = true; // treat overlay change as content change
}

void displayFlush(bool force)
{
    // Only signal that we need to refresh.
    if (dirty || force)
        flushRequested = true;
    if (force)
    {
        flushForceRequested = true;
        dirty = true; // force should render even if text did not change
    }
}

void displayTick()
{
    const uint32_t now = millis();

    // 1) fault recovery
    if (oledFault)
    {
        if (now < nextRecoverMs)
            return;

        if (recoverAttempts >= RECOVER_MAX_ATTEMPTS_BEFORE_LONG_PAUSE)
        {
            nextRecoverMs = now + RECOVER_LONG_PAUSE_MS;
            recoverAttempts = 0;
            return;
        }

        recoverAttempts++;
        bool ok = oledRecoverNow();
        if (ok)
        {
            oledFault = false;
        }
        else
        {
            nextRecoverMs = now + RECOVER_BACKOFF_MS;
            return;
        }
    }

    // 2) nothing to do or nothing to show
    if (!flushRequested)
        return;
    if (!dirty && overlayFn == nullptr)
        return;

    // 3) limiter
    if (!flushForceRequested && (now - lastFlushMs) < MIN_FLUSH_INTERVAL_MS)
        return;

    i2cTimeoutFlagClear();

    renderAll();

    lastFlushMs = now;
    flushRequested = false;
    flushForceRequested = false;

    const bool timedOut = i2cTimeoutFlagGet();
    const bool suspiciousLong = false;

    if (timedOut || suspiciousLong)
    {
        oledFault = true;
        nextRecoverMs = now + RECOVER_BACKOFF_MS;
        dirty = true;
    }
}
