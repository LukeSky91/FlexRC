#include <Arduino.h>
#include "controller/ui/settings_pages/set_photo.h"
#include "controller/ui/menu.h"
#include "controller/ui/ui_input.h"
#include "controller/photo_sensor.h"
#include "controller/buttons.h"
#include "controller/config.h"
#include "common/time_utils.h"

namespace
{
    enum class PhotoItem : uint8_t
    {
        MinAdc = 0,
        MaxAdc,
        MinLed,
        MaxLed,
        Source,
        Fixed,
        Filter,
        Hysteresis
    };

    enum class PhotoPage : uint8_t
    {
        Main = 0,
        Filter
    };

    uint32_t oledTick = 0;
    PhotoItem selected = PhotoItem::MinAdc;
    PhotoPage page = PhotoPage::Main;
    PhotoSensorConfig currentCfg{};
    PhotoSensorConfig originalCfg{};

    const char *modeLabel(PhotoBrightnessMode mode)
    {
        return (mode == PhotoBrightnessMode::Fixed) ? "FIXED" : "PHOTO";
    }

    const char *filterLabel(PhotoFilterLevel level)
    {
        switch (level)
        {
        case PhotoFilterLevel::Off:
            return "OFF";
        case PhotoFilterLevel::Low:
            return "LOW";
        case PhotoFilterLevel::Med:
            return "MED";
        case PhotoFilterLevel::High:
        default:
            return "HIGH";
        }
    }

    const char *hysteresisLabel(PhotoHysteresisLevel level)
    {
        switch (level)
        {
        case PhotoHysteresisLevel::Off:
            return "OFF";
        case PhotoHysteresisLevel::Low:
            return "LOW";
        case PhotoHysteresisLevel::Med:
            return "MED";
        case PhotoHysteresisLevel::High:
        default:
            return "HIGH";
        }
    }

    PhotoItem firstItemForPage(PhotoPage p)
    {
        return (p == PhotoPage::Main) ? PhotoItem::MinAdc : PhotoItem::Filter;
    }

    PhotoItem nextItemOnPage(PhotoItem item, PhotoPage p)
    {
        if (p == PhotoPage::Main)
        {
            switch (item)
            {
            case PhotoItem::MinAdc:
                return PhotoItem::MaxAdc;
            case PhotoItem::MaxAdc:
                return PhotoItem::MinLed;
            case PhotoItem::MinLed:
                return PhotoItem::MaxLed;
            case PhotoItem::MaxLed:
                return PhotoItem::Source;
            case PhotoItem::Source:
                return PhotoItem::Fixed;
            default:
                return PhotoItem::MinAdc;
            }
        }

        switch (item)
        {
        case PhotoItem::Filter:
            return PhotoItem::Hysteresis;
        default:
            return PhotoItem::Filter;
        }
    }

    void render(bool forceRedraw)
    {
        char line0[21], line1[21], line2[21], line3[21];
        char footerLeft[14];

        if (page == PhotoPage::Main)
        {
            const char aMin = (selected == PhotoItem::MinAdc) ? '>' : ' ';
            const char aMax = (selected == PhotoItem::MaxAdc) ? '>' : ' ';
            const char bMin = (selected == PhotoItem::MinLed) ? '>' : ' ';
            const char bMax = (selected == PhotoItem::MaxLed) ? '>' : ' ';
            const char src = (selected == PhotoItem::Source) ? '>' : ' ';
            const char fix = (selected == PhotoItem::Fixed) ? '>' : ' ';

            snprintf(line0, sizeof(line0), "ADC   %c%04d %c%04d", aMin, currentCfg.minRaw, aMax, currentCfg.maxRaw);
            snprintf(line1, sizeof(line1), "LED   %c%03u%% %c%03u%%", bMin, (unsigned)currentCfg.minLedPct, bMax, (unsigned)currentCfg.maxLedPct);
            snprintf(line2, sizeof(line2), "SRC   %c%s", src, modeLabel(currentCfg.mode));
            snprintf(line3, sizeof(line3), "FIX   %c%03u%%", fix, (unsigned)currentCfg.fixedPct);
        }
        else
        {
            const char filt = (selected == PhotoItem::Filter) ? '>' : ' ';
            const char hyst = (selected == PhotoItem::Hysteresis) ? '>' : ' ';

            snprintf(line0, sizeof(line0), "%c FILT %s", filt, filterLabel(currentCfg.filter));
            snprintf(line1, sizeof(line1), "%c HYST %s", hyst, hysteresisLabel(currentCfg.hysteresis));
            line2[0] = '\0';
            line3[0] = '\0';
        }

        snprintf(footerLeft, sizeof(footerLeft), "R%04d  %03u%%",
                 photoSensorReadRaw(),
                 (unsigned)photoSensorReadMappedPct());

        uiRenderPage(line0,
                     line1,
                     line2,
                     line3,
                     true,
                     (uint8_t)page + 1,
                     2,
                     buttonsLastReleaseKey(),
                     forceRedraw,
                     footerLeft);
    }

    void applyDelta(int delta)
    {
        switch (selected)
        {
        case PhotoItem::MinAdc:
        {
            int next = currentCfg.minRaw + delta;
            if (next < 0)
                next = currentCfg.maxRaw - 1;
            else if (next >= currentCfg.maxRaw)
                next = 0;
            currentCfg.minRaw = next;
            break;
        }
        case PhotoItem::MaxAdc:
        {
            int next = currentCfg.maxRaw + delta;
            if (next > ADC_MAX)
                next = currentCfg.minRaw + 1;
            else if (next <= currentCfg.minRaw)
                next = ADC_MAX;
            currentCfg.maxRaw = next;
            break;
        }
        case PhotoItem::MinLed:
        {
            int next = (int)currentCfg.minLedPct + delta;
            if (next < 0)
                next = currentCfg.maxLedPct;
            else if (next > (int)currentCfg.maxLedPct)
                next = 0;
            currentCfg.minLedPct = (uint8_t)next;
            break;
        }
        case PhotoItem::MaxLed:
        {
            int next = (int)currentCfg.maxLedPct + delta;
            if (next > 100)
                next = currentCfg.minLedPct;
            else if (next < (int)currentCfg.minLedPct)
                next = 100;
            currentCfg.maxLedPct = (uint8_t)next;
            break;
        }
        case PhotoItem::Source:
            currentCfg.mode = (currentCfg.mode == PhotoBrightnessMode::Sensor) ? PhotoBrightnessMode::Fixed : PhotoBrightnessMode::Sensor;
            break;
        case PhotoItem::Fixed:
        {
            int fixed = (int)currentCfg.fixedPct + delta;
            if (fixed < 0)
                fixed = 100;
            if (fixed > 100)
                fixed = 0;
            currentCfg.fixedPct = (uint8_t)fixed;
            break;
        }
        case PhotoItem::Filter:
        {
            int next = (int)currentCfg.filter + ((delta < 0) ? -1 : 1);
            if (next < 0)
                next = 3;
            if (next > 3)
                next = 0;
            currentCfg.filter = (PhotoFilterLevel)next;
            break;
        }
        case PhotoItem::Hysteresis:
        {
            int next = (int)currentCfg.hysteresis + ((delta < 0) ? -1 : 1);
            if (next < 0)
                next = 3;
            if (next > 3)
                next = 0;
            currentCfg.hysteresis = (PhotoHysteresisLevel)next;
            break;
        }
        }

        photoSensorSetConfig(currentCfg);
        currentCfg = photoSensorGetConfig();
    }
} // namespace

void setPhotoStart()
{
    uiInputReset();
    oledTick = 0;
    page = PhotoPage::Main;
    selected = PhotoItem::MinAdc;
    currentCfg = photoSensorGetConfig();
    originalCfg = currentCfg;
    render(true);
}

PhotoSettingsResult setPhotoLoop()
{
    const UiInputActions input = uiInputPoll();

    if (input.selectNext)
    {
        selected = nextItemOnPage(selected, page);
        render(true);
        return PhotoSettingsResult::Stay;
    }

    if (input.pagePrev)
    {
        page = (page == PhotoPage::Main) ? PhotoPage::Filter : PhotoPage::Main;
        selected = firstItemForPage(page);
        render(true);
        return PhotoSettingsResult::Stay;
    }

    if (input.pageNext)
    {
        page = (page == PhotoPage::Filter) ? PhotoPage::Main : PhotoPage::Filter;
        selected = firstItemForPage(page);
        render(true);
        return PhotoSettingsResult::Stay;
    }

    if (selected == PhotoItem::Source ||
        selected == PhotoItem::Filter ||
        selected == PhotoItem::Hysteresis)
    {
        if (input.dec || input.decFast)
        {
            applyDelta(-1);
            render(true);
            return PhotoSettingsResult::Stay;
        }
        if (input.inc || input.incFast)
        {
            applyDelta(1);
            render(true);
            return PhotoSettingsResult::Stay;
        }
    }
    else
    {
        const bool pctField = (selected == PhotoItem::MinLed || selected == PhotoItem::MaxLed || selected == PhotoItem::Fixed);
        if (input.dec)
        {
            applyDelta(pctField ? -1 : -10);
            render(true);
            return PhotoSettingsResult::Stay;
        }
        else if (input.inc)
        {
            applyDelta(pctField ? 1 : 10);
            render(true);
            return PhotoSettingsResult::Stay;
        }
        else if (input.decFast)
        {
            applyDelta(pctField ? -5 : -50);
            render(true);
            return PhotoSettingsResult::Stay;
        }
        else if (input.incFast)
        {
            applyDelta(pctField ? 5 : 50);
            render(true);
            return PhotoSettingsResult::Stay;
        }
    }

    if (input.enter)
    {
        photoSensorSetConfig(currentCfg);
        photoSensorSaveConfig();
        originalCfg = photoSensorGetConfig();
        currentCfg = originalCfg;
        render(true);
        return PhotoSettingsResult::Stay;
    }

    if (input.back)
    {
        photoSensorSetConfig(originalCfg);
        return PhotoSettingsResult::ExitToSettings;
    }

    if (!everyMs(DISPLAY_UI_REFRESH_INTERVAL_MS, oledTick))
        return PhotoSettingsResult::Stay;

    render(false);
    return PhotoSettingsResult::Stay;
}
