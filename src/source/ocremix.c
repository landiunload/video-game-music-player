#include "source/ocremix.h"

#include <windows.h>
#include <winhttp.h>
#include <shlwapi.h>
#include <wctype.h>
#include <stdlib.h>
#include <string.h>

static void SetError(wchar_t* error, size_t capacity, const wchar_t* message)
{
    if (error != NULL && capacity > 0)
        wcsncpy_s(error, capacity, message, _TRUNCATE);
}

static wchar_t* Utf8ToWide(const char* text, size_t size)
{
    if (text == NULL || size == 0 || size > INT_MAX) return NULL;
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        text, (int)size, NULL, 0);
    if (count <= 0) return NULL;
    wchar_t* result = HeapAlloc(GetProcessHeap(), 0,
        ((size_t)count + 1) * sizeof(wchar_t));
    if (result == NULL) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, text, (int)size, result, count);
    result[count] = L'\0';
    return result;
}

static bool CopyRange(wchar_t* destination, size_t capacity,
    const wchar_t* begin, const wchar_t* end)
{
    while (begin < end && iswspace(*begin)) ++begin;
    while (end > begin && iswspace(end[-1])) --end;
    if (begin == end || capacity == 0) return false;
    size_t length = (size_t)(end - begin);
    if (length >= capacity) length = capacity - 1;
    wmemcpy(destination, begin, length);
    destination[length] = L'\0';
    return true;
}

static bool ReadAttribute(const wchar_t* tagBegin, const wchar_t* tagEnd,
    const wchar_t* name, wchar_t* output, size_t capacity)
{
    size_t nameLength = wcslen(name);
    const wchar_t* cursor = tagBegin;
    while (cursor < tagEnd)
    {
        const wchar_t* match = StrStrIW(cursor, name);
        if (match == NULL || match >= tagEnd) return false;
        bool boundary = match == tagBegin || iswspace(match[-1])
            || match[-1] == L'<';
        const wchar_t* equals = match + nameLength;
        while (equals < tagEnd && iswspace(*equals)) ++equals;
        if (boundary && equals < tagEnd && *equals == L'=')
        {
            ++equals;
            while (equals < tagEnd && iswspace(*equals)) ++equals;
            if (equals >= tagEnd) return false;
            wchar_t quote = (*equals == L'\'' || *equals == L'\"')
                ? *equals++ : L' ';
            const wchar_t* end = equals;
            if (quote == L' ')
                while (end < tagEnd && !iswspace(*end) && *end != L'>') ++end;
            else
                while (end < tagEnd && *end != quote) ++end;
            return CopyRange(output, capacity, equals, end);
        }
        cursor = match + nameLength;
    }
    return false;
}

static void DecodeEntities(const wchar_t* source, wchar_t* output,
    size_t capacity)
{
    size_t written = 0;
    while (*source != L'\0' && written + 1 < capacity)
    {
        struct Entity { const wchar_t* value; wchar_t character; };
        static const struct Entity entities[] = {
            { L"&amp;", L'&' }, { L"&quot;", L'\"' },
            { L"&#39;", L'\'' }, { L"&apos;", L'\'' },
            { L"&lt;", L'<' }, { L"&gt;", L'>' },
            { L"&nbsp;", L' ' },
        };
        bool decoded = false;
        if (*source == L'&')
        {
            for (size_t i = 0; i < sizeof(entities) / sizeof(entities[0]); ++i)
            {
                size_t length = wcslen(entities[i].value);
                if (_wcsnicmp(source, entities[i].value, length) == 0)
                {
                    output[written++] = entities[i].character;
                    source += length;
                    decoded = true;
                    break;
                }
            }
            if (!decoded && source[1] == L'#')
            {
                wchar_t* end = NULL;
                unsigned long code = source[2] == L'x' || source[2] == L'X'
                    ? wcstoul(source + 3, &end, 16)
                    : wcstoul(source + 2, &end, 10);
                if (end != NULL && *end == L';' && code > 0 && code <= 0xffff)
                {
                    output[written++] = (wchar_t)code;
                    source = end + 1;
                    decoded = true;
                }
            }
        }
        if (!decoded) output[written++] = *source++;
    }
    output[written] = L'\0';
}

static bool FindMeta(const wchar_t* html, const wchar_t* property,
    wchar_t* output, size_t capacity)
{
    const wchar_t* cursor = html;
    while ((cursor = StrStrIW(cursor, L"<meta")) != NULL)
    {
        const wchar_t* end = wcschr(cursor, L'>');
        if (end == NULL) return false;
        wchar_t foundProperty[128];
        wchar_t content[TRACK_URL_CAPACITY];
        if (ReadAttribute(cursor, end, L"property", foundProperty, 128)
            && _wcsicmp(foundProperty, property) == 0
            && ReadAttribute(cursor, end, L"content", content,
                TRACK_URL_CAPACITY))
        {
            DecodeEntities(content, output, capacity);
            return output[0] != L'\0';
        }
        cursor = end + 1;
    }
    return false;
}

bool OcremixIsAllowedAudioUrl(const wchar_t* url)
{
    URL_COMPONENTS parts = { .dwStructSize = sizeof(parts) };
    wchar_t host[256];
    wchar_t path[TRACK_URL_CAPACITY];
    parts.lpszHostName = host;
    parts.dwHostNameLength = 256;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = TRACK_URL_CAPACITY;
    if (!WinHttpCrackUrl(url, 0, 0, &parts)
        || parts.nScheme != INTERNET_SCHEME_HTTPS) return false;
    host[parts.dwHostNameLength] = L'\0';
    path[parts.dwUrlPathLength] = L'\0';
    bool hostAllowed = _wcsicmp(host, L"iterations.org") == 0
        || _wcsicmp(host, L"ocrmirror.org") == 0
        || _wcsicmp(host, L"ocr.blueblue.fr") == 0;
    const wchar_t* extension = wcsrchr(path, L'.');
    return hostAllowed && extension != NULL
        && _wcsicmp(extension, L".mp3") == 0;
}

static bool FindAudio(const wchar_t* html, wchar_t* output, size_t capacity)
{
    const wchar_t* cursor = html;
    while ((cursor = StrStrIW(cursor, L"<a")) != NULL)
    {
        const wchar_t* end = wcschr(cursor, L'>');
        if (end == NULL) return false;
        wchar_t href[TRACK_URL_CAPACITY];
        if (ReadAttribute(cursor, end, L"href", href, TRACK_URL_CAPACITY)
            && OcremixIsAllowedAudioUrl(href))
        {
            wcsncpy_s(output, capacity, href, _TRUNCATE);
            return true;
        }
        cursor = end + 1;
    }
    return false;
}

static bool SplitTitle(const wchar_t* socialTitle, Track* track)
{
    wchar_t value[TRACK_ALBUM_CAPACITY + TRACK_TITLE_CAPACITY];
    wcsncpy_s(value,
        sizeof(value) / sizeof(value[0]), socialTitle, _TRUNCATE);
    const wchar_t suffix[] = L" OC ReMix";
    size_t length = wcslen(value);
    size_t suffixLength = wcslen(suffix);
    if (length > suffixLength
        && _wcsicmp(value + length - suffixLength, suffix) == 0)
        value[length - suffixLength] = L'\0';
    const wchar_t* quoteBegin = wcschr(value, L'\"');
    const wchar_t* quoteEnd = wcsrchr(value, L'\"');
    if (quoteBegin != NULL && quoteEnd != NULL && quoteEnd > quoteBegin)
    {
        return CopyRange(track->album, TRACK_ALBUM_CAPACITY,
                value, quoteBegin)
            && CopyRange(track->title, TRACK_TITLE_CAPACITY,
                quoteBegin + 1, quoteEnd);
    }
    wcsncpy_s(track->title, TRACK_TITLE_CAPACITY, value, _TRUNCATE);
    wcscpy_s(track->album, TRACK_ALBUM_CAPACITY, L"OverClocked ReMix");
    return track->title[0] != L'\0';
}

bool OcremixParseTrackPage(const char* htmlBytes, size_t htmlSize,
    const wchar_t* finalPageUrl, Track* outTrack,
    wchar_t* error, size_t errorCapacity)
{
    if (htmlBytes == NULL || htmlSize == 0 || finalPageUrl == NULL
        || outTrack == NULL)
    {
        SetError(error, errorCapacity, L"Некорректная страница OC ReMix.");
        return false;
    }
    memset(outTrack, 0, sizeof(*outTrack));
    wchar_t* html = Utf8ToWide(htmlBytes, htmlSize);
    if (html == NULL)
    {
        SetError(error, errorCapacity, L"Не удалось прочитать страницу OC ReMix.");
        return false;
    }
    wchar_t socialTitle[TRACK_ALBUM_CAPACITY + TRACK_TITLE_CAPACITY];
    bool ok = FindMeta(html, L"og:title", socialTitle,
            sizeof(socialTitle) / sizeof(socialTitle[0]))
        && SplitTitle(socialTitle, outTrack)
        && FindAudio(html, outTrack->audioUrl, TRACK_URL_CAPACITY);
    if (!ok)
    {
        SetError(error, errorCapacity,
            L"Страница OC ReMix не содержит доступного MP3.");
        HeapFree(GetProcessHeap(), 0, html);
        return false;
    }
    FindMeta(html, L"og:image", outTrack->artworkUrl, TRACK_URL_CAPACITY);
    wcsncpy_s(outTrack->pageUrl, TRACK_URL_CAPACITY,
        finalPageUrl, _TRUNCATE);
    outTrack->id = TrackHashText(outTrack->pageUrl);
    outTrack->local = false;
    SetError(error, errorCapacity, L"");
    HeapFree(GetProcessHeap(), 0, html);
    return true;
}

