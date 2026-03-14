#include <Preferences.h>
#include "controller/storage.h"

namespace
{
constexpr const char *kStorageNamespace = "flexrc";
}

bool storageInit()
{
    Preferences prefs;
    const bool ok = prefs.begin(kStorageNamespace, false);
    prefs.end();
    return ok;
}

bool storageReadBlob(const char *key, void *data, size_t size)
{
    if (!key || !data || size == 0)
        return false;

    Preferences prefs;
    if (!prefs.begin(kStorageNamespace, true))
        return false;

    const size_t read = prefs.getBytes(key, data, size);
    prefs.end();
    return read == size;
}

bool storageWriteBlob(const char *key, const void *data, size_t size)
{
    if (!key || !data || size == 0)
        return false;

    Preferences prefs;
    if (!prefs.begin(kStorageNamespace, false))
        return false;

    const size_t written = prefs.putBytes(key, data, size);
    prefs.end();
    return written == size;
}
