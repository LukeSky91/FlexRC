#include <Arduino.h>
#include "controller/ui/settings_pages/calib_joy.h"
#include "controller/display.h"
#include "controller/buttons.h"
#include "controller/joysticks.h"
#include "common/time_utils.h"

static uint32_t calibTick = 0;
static uint32_t calibTickCenter = 0;

enum class CalStick : uint8_t
{
    Left = 0,
    Right = 1
};
enum class CalSel : uint8_t
{
    Extents = 0,
    Center = 1
};
enum class CalPage : uint8_t
{
    LeftLive = 0,
    LeftStored = 1,
    RightLive = 2,
    RightStored = 3
};

static CalStick curStick = CalStick::Left;
static CalSel curSel = CalSel::Extents;
static CalPage curPage = CalPage::LeftLive;
static bool extentsStartedL = false;
static bool extentsStartedR = false;

struct CalBackup
{
    int minX, maxX, minY, maxY, centerX, centerY;
};

static CalBackup origL{};
static CalBackup origR{};
static bool savedAny = false;
static int lastCtrX = 0;
static int lastCtrY = 0;
static uint32_t saveUntilMs = 0;
static bool armDown = false;
static bool armLeft = false;
static bool armRight = false;
static bool armUp = false;
static bool armCenter = false;

static void render(bool forceRedraw);
static void snapshotStored()
{
    origL = {joyL.getCalMinX(), joyL.getCalMaxX(), joyL.getCalMinY(), joyL.getCalMaxY(), joyL.getCenterX(), joyL.getCenterY()};
    origR = {joyR.getCalMinX(), joyR.getCalMaxX(), joyR.getCalMinY(), joyR.getCalMaxY(), joyR.getCenterX(), joyR.getCenterY()};
}

static Joystick &stickRef(CalStick s)
{
    return (s == CalStick::Left) ? joyL : joyR;
}

static bool &extentsFlag(CalStick s)
{
    return (s == CalStick::Left) ? extentsStartedL : extentsStartedR;
}

static bool isStoredPage()
{
    return curPage == CalPage::LeftStored || curPage == CalPage::RightStored;
}

static void startExtentsIfNeeded()
{
    if (curSel == CalSel::Extents)
    {
        bool &flag = extentsFlag(curStick);
        if (!flag)
        {
            stickRef(curStick).startCalibration();
            flag = true;
        }
    }
}

static void restoreOriginal()
{
    joyL.setCalibration(origL.minX, origL.maxX, origL.minY, origL.maxY);
    joyR.setCalibration(origR.minX, origR.maxX, origR.minY, origR.maxY);
    joyL.setCenter(origL.centerX, origL.centerY);
    joyR.setCenter(origR.centerX, origR.centerY);
}

static void setPage(CalPage p)
{
    curPage = p;
    curStick = (curPage == CalPage::LeftLive || curPage == CalPage::LeftStored) ? CalStick::Left : CalStick::Right;
    curSel = CalSel::Extents; // after page change go back to EXT
    // refresh CTR displayed for the new stick
    Joystick &j = stickRef(curStick);
    lastCtrX = j.getCenterX();
    lastCtrY = j.getCenterY();
    if (!isStoredPage())
    {
        startExtentsIfNeeded();
    }
    // flush any stale releases after page switch
    (void)keyReleased(Key::Down);
    (void)keyReleased(Key::Left);
    (void)keyReleased(Key::Right);
    (void)keyReleased(Key::Up);
    (void)keyReleased(Key::Center);
    armDown = !keyDown(Key::Down);
    armLeft = !keyDown(Key::Left);
    armRight = !keyDown(Key::Right);
    armUp = !keyDown(Key::Up);
    armCenter = !keyDown(Key::Center);
    render(true);
}

static void render(bool forceRedraw)
{
    char line0[21], line1[21], line2[21], line3[21], line4[21];

    const bool storedView = isStoredPage();
    Joystick &j = stickRef(curStick);
    const char stickChar = (curStick == CalStick::Left) ? 'L' : 'R';
    const char selExt = (curSel == CalSel::Extents) ? '>' : ' ';
    const char selCtr = (curSel == CalSel::Center) ? '>' : ' ';
    const uint8_t pageIdx = static_cast<uint8_t>(curPage) + 1;
    const uint8_t totalPages = 4;

    int dispMinX, dispMaxX, dispMinY, dispMaxY, dispCtrX, dispCtrY;
    if (storedView)
    {
        const CalBackup &b = (curStick == CalStick::Left) ? origL : origR;
        dispMinX = b.minX;
        dispMaxX = b.maxX;
        dispMinY = b.minY;
        dispMaxY = b.maxY;
        dispCtrX = b.centerX;
        dispCtrY = b.centerY;
    }
    else
    {
        dispMinX = j.getCalMinX();
        dispMaxX = j.getCalMaxX();
        dispMinY = j.getCalMinY();
        dispMaxY = j.getCalMaxY();
        dispCtrX = lastCtrX;
        dispCtrY = lastCtrY;
    }

    if (!storedView && curSel == CalSel::Center)
    {
        // update both channels only in CTR mode
        lastCtrX = j.readRawInvertedX();
        lastCtrY = j.readRawInvertedY();
        dispCtrX = lastCtrX;
        dispCtrY = lastCtrY;
    }

    snprintf(line0, sizeof(line0), "%cEXT X %04d - %04d", storedView ? ' ' : selExt, dispMinX, dispMaxX);
    snprintf(line1, sizeof(line1), "     Y %04d - %04d", dispMinY, dispMaxY);
    snprintf(line2, sizeof(line2), "%cCTR X %04d", storedView ? ' ' : selCtr, dispCtrX);
    snprintf(line3, sizeof(line3), "     Y %04d", dispCtrY);

    bool showSave = (saveUntilMs != 0) && (millis() < saveUntilMs);
    if (!showSave)
        saveUntilMs = 0;

    if (showSave)
        snprintf(line4, sizeof(line4), " %c     SAVE    [%u/%u]", stickChar, pageIdx, totalPages);
    else
        snprintf(line4, sizeof(line4), " %c             [%u/%u]", stickChar, pageIdx, totalPages);

    displayText(0, line0);
    displayText(1, line1);
    displayText(2, line2);
    displayText(3, line3);
    displayText(4, line4);
    displayFlush(forceRedraw);
}

void calibJoyStart()
{
    calibTick = 0;
    calibTickCenter = 0;
    buttonsConsumeAll();
    (void)keyReleased(Key::Down);
    (void)keyReleased(Key::Left);
    (void)keyReleased(Key::Right);
    (void)keyReleased(Key::Up);
    (void)keyReleased(Key::Center);
    armDown = !keyDown(Key::Down);
    armLeft = !keyDown(Key::Left);
    armRight = !keyDown(Key::Right);
    armUp = !keyDown(Key::Up);
    armCenter = !keyDown(Key::Center);
    curStick = CalStick::Left;
    curSel = CalSel::Extents;
    curPage = CalPage::LeftLive;
    extentsStartedL = false;
    extentsStartedR = false;
    savedAny = false;
    saveUntilMs = 0;

    lastCtrX = joyL.getCenterX();
    lastCtrY = joyL.getCenterY();

    // backup current calibration in case of exit without save
    snapshotStored();

    startExtentsIfNeeded();
    render(true);
}

CalibrationResult calibJoyLoop()
{
    const bool storedView = isStoredPage();

    if (!storedView)
        startExtentsIfNeeded();

    // auto-arm after release
    if (!armDown && !keyDown(Key::Down))
        armDown = true;
    if (!armLeft && !keyDown(Key::Left))
        armLeft = true;
    if (!armRight && !keyDown(Key::Right))
        armRight = true;
    if (!armUp && !keyDown(Key::Up))
        armUp = true;
    if (!armCenter && !keyDown(Key::Center))
        armCenter = true;

    if (!storedView && curSel == CalSel::Extents)
    {
        stickRef(curStick).updateCalibrationSample();
    }

    // DOWN: exit; if nothing was saved, restore original
    if (armDown && keyReleased(Key::Down))
    {
        if (!savedAny)
            restoreOriginal();
        return CalibrationResult::ExitToMain;
    }

    if (armLeft && keyReleased(Key::Left))
    {
        uint8_t p = static_cast<uint8_t>(curPage);
        p = (p == 0) ? 3 : (p - 1);
        setPage(static_cast<CalPage>(p));
        return CalibrationResult::Running;
    }
    if (armRight && keyReleased(Key::Right))
    {
        uint8_t p = static_cast<uint8_t>(curPage);
        p = (uint8_t)((p + 1) % 4);
        setPage(static_cast<CalPage>(p));
        return CalibrationResult::Running;
    }

    if (!storedView && armUp && keyReleased(Key::Up))
    {
        curSel = (curSel == CalSel::Extents) ? CalSel::Center : CalSel::Extents;
        startExtentsIfNeeded();
        render(true);
        return CalibrationResult::Running;
    }

    if (!storedView && armCenter && keyReleased(Key::Center))
    {
        Joystick &j = stickRef(curStick);
        if (curSel == CalSel::Extents)
        {
            j.finishCalibration();
            curSel = CalSel::Center; // after save move to CTR
        }
        else // Center
        {
            int cx = j.readRawInvertedX();
            int cy = j.readRawInvertedY();
            j.recenterAround(cx, cy);
            lastCtrX = cx;
            lastCtrY = cy;
            curSel = CalSel::Extents; // after save return to EXT
        }
        joysticksSaveCalibration();
        snapshotStored();
        savedAny = true;
        saveUntilMs = millis() + 1200; // show SAVE for ~1.2s
        render(true);
        return CalibrationResult::Running;
    }

    if (storedView)
    {
        if (!everyMs(500, calibTick))
            return CalibrationResult::Running;
    }
    else if (curSel == CalSel::Extents)
    {
        if (!everyMs(350, calibTick))
            return CalibrationResult::Running;
    }
    else
    {
        if (!everyMs(500, calibTickCenter))
            return CalibrationResult::Running;
    }

    render(false);
    return CalibrationResult::Running;
}
