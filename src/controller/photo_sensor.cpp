#include <Arduino.h>

#include "controller/photo_sensor.h"
#include "controller/config.h"
#include "controller/storage.h"

namespace
{
struct StoredPhotoConfig
{
    uint16_t magic;
    uint16_t minRaw;
    uint16_t maxRaw;
    uint8_t minLedPct;
    uint8_t maxLedPct;
    uint8_t mode;
    uint8_t fixedPct;
    uint8_t filter;
    uint8_t hysteresis;
    uint16_t crc;
};

constexpr uint16_t kPhotoMagic = 0x5048;
constexpr const char *kStorageKeyPhoto = "photo_cfg";

PhotoSensorConfig g_config{
    0,
    ADC_MAX,
    0,
    100,
    PhotoBrightnessMode::Sensor,
    35,
    PhotoFilterLevel::Med,
    PhotoHysteresisLevel::Low};

bool g_filterInitialized = false;
uint32_t g_lastFilterMs = 0;
int32_t g_filteredPctX256 = 0;
uint8_t g_lastOutputPct = 0;

uint16_t crcPhoto(const StoredPhotoConfig &d)
{
    return (uint16_t)(d.magic ^ d.minRaw ^ d.maxRaw ^ d.minLedPct ^ d.maxLedPct ^
                      d.mode ^ d.fixedPct ^ d.filter ^ d.hysteresis ^ 0x5A5A);
}

PhotoSensorConfig sanitizeConfig(PhotoSensorConfig cfg)
{
    if (cfg.minRaw < 0)
        cfg.minRaw = 0;
    if (cfg.maxRaw > ADC_MAX)
        cfg.maxRaw = ADC_MAX;
    if (cfg.maxRaw < 1)
        cfg.maxRaw = 1;
    if (cfg.minRaw > ADC_MAX - 1)
        cfg.minRaw = ADC_MAX - 1;
    if (cfg.minRaw >= cfg.maxRaw)
    {
        if (cfg.maxRaw < ADC_MAX)
            cfg.maxRaw = cfg.minRaw + 1;
        else
            cfg.minRaw = cfg.maxRaw - 1;
    }

    if (cfg.minLedPct > 100)
        cfg.minLedPct = 100;
    if (cfg.maxLedPct > 100)
        cfg.maxLedPct = 100;
    if (cfg.minLedPct > cfg.maxLedPct)
    {
        uint8_t t = cfg.minLedPct;
        cfg.minLedPct = cfg.maxLedPct;
        cfg.maxLedPct = t;
    }

    if (cfg.fixedPct > 100)
        cfg.fixedPct = 100;
    if (cfg.mode != PhotoBrightnessMode::Sensor && cfg.mode != PhotoBrightnessMode::Fixed)
        cfg.mode = PhotoBrightnessMode::Sensor;
    if ((uint8_t)cfg.filter > (uint8_t)PhotoFilterLevel::High)
        cfg.filter = PhotoFilterLevel::Med;
    if ((uint8_t)cfg.hysteresis > (uint8_t)PhotoHysteresisLevel::High)
        cfg.hysteresis = PhotoHysteresisLevel::Low;
    return cfg;
}

uint8_t mapRawToPct(int raw, int minRaw, int maxRaw)
{
    long pct = (long)(raw - minRaw) * 100 / (long)(maxRaw - minRaw);
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    return (uint8_t)pct;
}

uint8_t mapRawToLedPct(int raw, const PhotoSensorConfig &cfg)
{
    const uint8_t basePct = mapRawToPct(raw, cfg.minRaw, cfg.maxRaw);
    long ledPct = (long)cfg.minLedPct + ((long)basePct * (long)(cfg.maxLedPct - cfg.minLedPct)) / 100L;
    if (ledPct < 0)
        ledPct = 0;
    if (ledPct > 100)
        ledPct = 100;
    return (uint8_t)ledPct;
}

uint8_t filterShift(PhotoFilterLevel level)
{
    switch (level)
    {
    case PhotoFilterLevel::Off:
        return 0;
    case PhotoFilterLevel::Low:
        return 3; // 1/8
    case PhotoFilterLevel::Med:
        return 4; // 1/16
    case PhotoFilterLevel::High:
    default:
        return 5; // 1/32
    }
}

uint8_t hysteresisThreshold(PhotoHysteresisLevel level)
{
    switch (level)
    {
    case PhotoHysteresisLevel::Off:
        return 0;
    case PhotoHysteresisLevel::Low:
        return 2;
    case PhotoHysteresisLevel::Med:
        return 4;
    case PhotoHysteresisLevel::High:
    default:
        return 6;
    }
}
} // namespace

void photoSensorInit()
{
    pinMode(PHOTO_PIN, INPUT);

    StoredPhotoConfig stored{};
    if (storageReadBlob(kStorageKeyPhoto, &stored, sizeof(stored)) &&
        stored.magic == kPhotoMagic &&
        stored.crc == crcPhoto(stored))
    {
        g_config.minRaw = stored.minRaw;
        g_config.maxRaw = stored.maxRaw;
        g_config.minLedPct = stored.minLedPct;
        g_config.maxLedPct = stored.maxLedPct;
        g_config.mode = (stored.mode == 1) ? PhotoBrightnessMode::Fixed : PhotoBrightnessMode::Sensor;
        g_config.fixedPct = stored.fixedPct;
        g_config.filter = (PhotoFilterLevel)stored.filter;
        g_config.hysteresis = (PhotoHysteresisLevel)stored.hysteresis;
        g_config = sanitizeConfig(g_config);
    }

    g_filterInitialized = false;
}

int photoSensorReadRaw()
{
    int raw = analogRead(PHOTO_PIN);
    if (raw < 0)
        raw = 0;
    if (raw > ADC_MAX)
        raw = ADC_MAX;
    return raw;
}

uint8_t photoSensorReadPct()
{
    const PhotoSensorConfig cfg = sanitizeConfig(g_config);
    return mapRawToPct(photoSensorReadRaw(), cfg.minRaw, cfg.maxRaw);
}

uint8_t photoSensorReadMappedPct()
{
    const PhotoSensorConfig cfg = sanitizeConfig(g_config);
    return mapRawToLedPct(photoSensorReadRaw(), cfg);
}

uint8_t photoSensorLedBrightnessPct()
{
    const PhotoSensorConfig cfg = sanitizeConfig(g_config);
    if (cfg.mode == PhotoBrightnessMode::Fixed)
        return cfg.fixedPct;

    const uint8_t livePct = mapRawToLedPct(photoSensorReadRaw(), cfg);
    const uint32_t now = millis();

    if (!g_filterInitialized)
    {
        g_filterInitialized = true;
        g_lastFilterMs = now;
        g_filteredPctX256 = (int32_t)livePct << 8;
        g_lastOutputPct = livePct;
        return livePct;
    }

    if (now - g_lastFilterMs >= 50)
    {
        g_lastFilterMs = now;
        const uint8_t shift = filterShift(cfg.filter);
        const int32_t target = (int32_t)livePct << 8;
        if (shift == 0)
            g_filteredPctX256 = target;
        else
            g_filteredPctX256 += (target - g_filteredPctX256) >> shift;
    }

    const uint8_t filteredPct = (uint8_t)((g_filteredPctX256 + 128) >> 8);
    const uint8_t threshold = hysteresisThreshold(cfg.hysteresis);
    if (threshold == 0 || abs((int)filteredPct - (int)g_lastOutputPct) >= (int)threshold)
        g_lastOutputPct = filteredPct;

    return g_lastOutputPct;
}

PhotoSensorConfig photoSensorGetConfig()
{
    return sanitizeConfig(g_config);
}

void photoSensorSetConfig(const PhotoSensorConfig &cfg)
{
    g_config = sanitizeConfig(cfg);
    g_filterInitialized = false;
}

void photoSensorSaveConfig()
{
    g_config = sanitizeConfig(g_config);

    StoredPhotoConfig stored{};
    stored.magic = kPhotoMagic;
    stored.minRaw = (uint16_t)g_config.minRaw;
    stored.maxRaw = (uint16_t)g_config.maxRaw;
    stored.minLedPct = g_config.minLedPct;
    stored.maxLedPct = g_config.maxLedPct;
    stored.mode = (g_config.mode == PhotoBrightnessMode::Fixed) ? 1 : 0;
    stored.fixedPct = g_config.fixedPct;
    stored.filter = (uint8_t)g_config.filter;
    stored.hysteresis = (uint8_t)g_config.hysteresis;
    stored.crc = crcPhoto(stored);
    storageWriteBlob(kStorageKeyPhoto, &stored, sizeof(stored));
}
