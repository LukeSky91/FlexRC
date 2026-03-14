#pragma once

#include <stddef.h>

bool storageInit();
bool storageReadBlob(const char *key, void *data, size_t size);
bool storageWriteBlob(const char *key, const void *data, size_t size);
