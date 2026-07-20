#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

#define TRACK_TITLE_CAPACITY 256
#define TRACK_ALBUM_CAPACITY 384
#define TRACK_URL_CAPACITY 2048
#define TRACK_PATH_CAPACITY 1024

typedef struct Track
{
    uint64_t id;
    wchar_t title[TRACK_TITLE_CAPACITY];
    wchar_t album[TRACK_ALBUM_CAPACITY];
    wchar_t pageUrl[TRACK_URL_CAPACITY];
    wchar_t audioUrl[TRACK_URL_CAPACITY];
    wchar_t artworkUrl[TRACK_URL_CAPACITY];
    wchar_t audioPath[TRACK_PATH_CAPACITY];
    wchar_t artworkPath[TRACK_PATH_CAPACITY];
    bool local;
} Track;

uint64_t TrackHashText(const wchar_t* text);
void TrackFromLocalFile(Track* track, const wchar_t* path);

