#pragma once

#include "track.h"

#include <stdbool.h>
#include <stddef.h>

// Разбор отделён от HTTP, чтобы формат источника можно было проверять
// обычными unit-тестами без сети.
bool KhinsiderParseTrackPage(const char* html, size_t htmlSize,
    const wchar_t* finalPageUrl, Track* outTrack,
    wchar_t* error, size_t errorCapacity);

bool KhinsiderIsAllowedAudioUrl(const wchar_t* url);

