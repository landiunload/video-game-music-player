#include "track.h"

#include <windows.h>
#include <string.h>

uint64_t TrackHashText(const wchar_t* text)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    if (text == NULL) return hash;
    while (*text != L'\0')
    {
        uint16_t value = (uint16_t)*text++;
        hash ^= (uint8_t)(value & 255u);
        hash *= UINT64_C(1099511628211);
        hash ^= (uint8_t)(value >> 8);
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

void TrackFromLocalFile(Track* track, const wchar_t* path)
{
    if (track == NULL || path == NULL) return;
    memset(track, 0, sizeof(*track));
    wchar_t fullPath[TRACK_PATH_CAPACITY];
    DWORD length = GetFullPathNameW(path, TRACK_PATH_CAPACITY, fullPath, NULL);
    const wchar_t* source = length > 0 && length < TRACK_PATH_CAPACITY
        ? fullPath : path;
    wcsncpy_s(track->audioPath, TRACK_PATH_CAPACITY, source, _TRUNCATE);

    const wchar_t* name = wcsrchr(source, L'\\');
    name = name == NULL ? source : name + 1;
    wcsncpy_s(track->title, TRACK_TITLE_CAPACITY, name, _TRUNCATE);
    wchar_t* extension = wcsrchr(track->title, L'.');
    if (extension != NULL) *extension = L'\0';
    wcscpy_s(track->album, TRACK_ALBUM_CAPACITY, L"Локальная музыка");
    track->id = TrackHashText(source);
    track->local = true;
}
