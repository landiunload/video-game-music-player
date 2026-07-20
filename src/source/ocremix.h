#pragma once

#include "track.h"

#include <stdbool.h>
#include <stddef.h>

bool OcremixParseTrackPage(const char* html, size_t htmlSize,
    const wchar_t* finalPageUrl, Track* outTrack,
    wchar_t* error, size_t errorCapacity);

bool OcremixIsAllowedAudioUrl(const wchar_t* url);

