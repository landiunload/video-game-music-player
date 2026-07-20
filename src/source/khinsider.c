#include "source/khinsider.h"

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
    if (count <= 0)
        count = MultiByteToWideChar(CP_ACP, 0, text, (int)size, NULL, 0);
    if (count <= 0) return NULL;
    wchar_t* result = HeapAlloc(GetProcessHeap(), 0,
        ((size_t)count + 1) * sizeof(wchar_t));
    if (result == NULL) return NULL;
    UINT codePage = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        text, (int)size, NULL, 0) > 0 ? CP_UTF8 : CP_ACP;
    MultiByteToWideChar(codePage, 0, text, (int)size, result, count);
    result[count] = L'\0';
    return result;
}

static bool StartsWithInsensitive(const wchar_t* value, const wchar_t* prefix)
{
    while (*prefix != L'\0')
    {
        if (*value == L'\0' || towlower(*value) != towlower(*prefix))
            return false;
        ++value;
        ++prefix;
    }
    return true;
}

static bool ContainsInsensitive(const wchar_t* value, const wchar_t* needle)
{
    size_t needleLength = wcslen(needle);
    for (; *value != L'\0'; ++value)
        if (_wcsnicmp(value, needle, needleLength) == 0) return true;
    return false;
}

static void DecodeEntity(const wchar_t** cursor, wchar_t** output)
{
    const wchar_t* input = *cursor;
    struct NamedEntity { const wchar_t* encoded; wchar_t decoded; };
    static const struct NamedEntity entities[] = {
        { L"&amp;", L'&' }, { L"&quot;", L'\"' }, { L"&#39;", L'\'' },
        { L"&apos;", L'\'' }, { L"&lt;", L'<' }, { L"&gt;", L'>' },
        { L"&nbsp;", L' ' },
    };
    for (size_t i = 0; i < sizeof(entities) / sizeof(entities[0]); ++i)
    {
        size_t length = wcslen(entities[i].encoded);
        if (_wcsnicmp(input, entities[i].encoded, length) == 0)
        {
            *(*output)++ = entities[i].decoded;
            *cursor += length - 1;
            return;
        }
    }
    if (input[1] == L'#')
    {
        wchar_t* end = NULL;
        unsigned long code = 0;
        if (input[2] == L'x' || input[2] == L'X')
            code = wcstoul(input + 3, &end, 16);
        else
            code = wcstoul(input + 2, &end, 10);
        if (end != NULL && *end == L';' && code > 0 && code <= 0xffff)
        {
            *(*output)++ = (wchar_t)code;
            *cursor = end;
            return;
        }
    }
    *(*output)++ = L'&';
}

static wchar_t* VisibleText(const wchar_t* html)
{
    size_t length = wcslen(html);
    wchar_t* result = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
        (length + 1) * sizeof(wchar_t));
    if (result == NULL) return NULL;
    bool inTag = false;
    bool pendingSpace = false;
    wchar_t* output = result;
    for (const wchar_t* cursor = html; *cursor != L'\0'; ++cursor)
    {
        if (*cursor == L'<') { inTag = true; pendingSpace = true; continue; }
        if (inTag)
        {
            if (*cursor == L'>') inTag = false;
            continue;
        }
        if (iswspace(*cursor)) { pendingSpace = true; continue; }
        if (pendingSpace && output != result && output[-1] != L' ')
            *output++ = L' ';
        pendingSpace = false;
        if (*cursor == L'&') DecodeEntity(&cursor, &output);
        else *output++ = *cursor;
    }
    while (output > result && output[-1] == L' ') --output;
    *output = L'\0';
    return result;
}

static bool CopyTrimmedRange(wchar_t* destination, size_t capacity,
    const wchar_t* begin, const wchar_t* end)
{
    while (begin < end && iswspace(*begin)) ++begin;
    while (end > begin && iswspace(end[-1])) --end;
    size_t length = (size_t)(end - begin);
    if (length == 0 || capacity == 0) return false;
    if (length >= capacity) length = capacity - 1;
    wmemcpy(destination, begin, length);
    destination[length] = L'\0';
    return true;
}

static bool ExtractTextBetween(const wchar_t* text, const wchar_t* beginMarker,
    const wchar_t* endMarker, wchar_t* output, size_t outputCapacity)
{
    const wchar_t* begin = StrStrIW(text, beginMarker);
    if (begin == NULL) return false;
    begin += wcslen(beginMarker);
    const wchar_t* end = StrStrIW(begin, endMarker);
    if (end == NULL) return false;
    return CopyTrimmedRange(output, outputCapacity, begin, end);
}

static bool ReadAttribute(const wchar_t* tagBegin, const wchar_t* tagEnd,
    const wchar_t* attribute, wchar_t* output, size_t outputCapacity)
{
    size_t attributeLength = wcslen(attribute);
    const wchar_t* cursor = tagBegin;
    while (cursor < tagEnd)
    {
        const wchar_t* match = StrStrIW(cursor, attribute);
        if (match == NULL || match >= tagEnd) return false;
        bool boundaryBefore = match == tagBegin
            || iswspace(match[-1]) || match[-1] == L'<';
        const wchar_t* equals = match + attributeLength;
        while (equals < tagEnd && iswspace(*equals)) ++equals;
        if (boundaryBefore && equals < tagEnd && *equals == L'=')
        {
            ++equals;
            while (equals < tagEnd && iswspace(*equals)) ++equals;
            if (equals >= tagEnd) return false;
            wchar_t quote = (*equals == L'\'' || *equals == L'\"')
                ? *equals++ : L' ';
            const wchar_t* end = equals;
            if (quote == L' ') while (end < tagEnd && !iswspace(*end) && *end != L'>') ++end;
            else while (end < tagEnd && *end != quote) ++end;
            return CopyTrimmedRange(output, outputCapacity, equals, end);
        }
        cursor = match + attributeLength;
    }
    return false;
}

static bool MakeAbsoluteUrl(const wchar_t* base, const wchar_t* value,
    wchar_t* output, size_t capacity)
{
    if (StartsWithInsensitive(value, L"https://")
        || StartsWithInsensitive(value, L"http://"))
    {
        wcsncpy_s(output, capacity, value, _TRUNCATE);
        return true;
    }
    DWORD resultLength = (DWORD)capacity;
    return UrlCombineW(base, value, output, &resultLength,
        URL_ESCAPE_UNSAFE) == S_OK;
}

bool KhinsiderIsAllowedAudioUrl(const wchar_t* url)
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
    size_t hostLength = wcslen(host);
    bool allowedHost = _wcsicmp(host, L"khinsider.com") == 0
        || (hostLength > 14 && _wcsicmp(host + hostLength - 14, L".khinsider.com") == 0)
        || _wcsicmp(host, L"vgmtreasurechest.com") == 0
        || (hostLength > 21 && _wcsicmp(host + hostLength - 21, L".vgmtreasurechest.com") == 0);
    if (!allowedHost) return false;
    const wchar_t* extension = wcsrchr(path, L'.');
    return extension != NULL && (_wcsicmp(extension, L".mp3") == 0
        || _wcsicmp(extension, L".ogg") == 0
        || _wcsicmp(extension, L".m4a") == 0);
}

static bool FindAudioLink(const wchar_t* html, const wchar_t* pageUrl,
    wchar_t* output, size_t capacity)
{
    const wchar_t* cursor = html;
    while ((cursor = StrStrIW(cursor, L"<a")) != NULL)
    {
        const wchar_t* end = wcschr(cursor, L'>');
        if (end == NULL) break;
        wchar_t href[TRACK_URL_CAPACITY];
        if (ReadAttribute(cursor, end, L"href", href, TRACK_URL_CAPACITY))
        {
            wchar_t absolute[TRACK_URL_CAPACITY];
            if (MakeAbsoluteUrl(pageUrl, href, absolute, TRACK_URL_CAPACITY)
                && KhinsiderIsAllowedAudioUrl(absolute))
            {
                wcsncpy_s(output, capacity, absolute, _TRUNCATE);
                return true;
            }
        }
        cursor = end + 1;
    }
    return false;
}

static bool FindArtwork(const wchar_t* html, const wchar_t* pageUrl,
    wchar_t* output, size_t capacity)
{
    const wchar_t* cursor = html;
    while ((cursor = StrStrIW(cursor, L"<meta")) != NULL)
    {
        const wchar_t* end = wcschr(cursor, L'>');
        if (end == NULL) break;
        wchar_t property[128];
        wchar_t content[TRACK_URL_CAPACITY];
        bool hasProperty = ReadAttribute(cursor, end, L"property", property, 128)
            || ReadAttribute(cursor, end, L"name", property, 128);
        if (hasProperty
            && (_wcsicmp(property, L"og:image") == 0
                || _wcsicmp(property, L"twitter:image") == 0)
            && ReadAttribute(cursor, end, L"content", content, TRACK_URL_CAPACITY))
            return MakeAbsoluteUrl(pageUrl, content, output, capacity);
        cursor = end + 1;
    }
    return false;
}

bool KhinsiderParseTrackPage(const char* htmlBytes, size_t htmlSize,
    const wchar_t* finalPageUrl, Track* outTrack,
    wchar_t* error, size_t errorCapacity)
{
    if (htmlBytes == NULL || htmlSize == 0 || finalPageUrl == NULL
        || outTrack == NULL)
    {
        SetError(error, errorCapacity, L"Некорректная страница источника.");
        return false;
    }
    memset(outTrack, 0, sizeof(*outTrack));
    wchar_t* html = Utf8ToWide(htmlBytes, htmlSize);
    wchar_t* visible = html == NULL ? NULL : VisibleText(html);
    if (html == NULL || visible == NULL)
    {
        if (html != NULL) HeapFree(GetProcessHeap(), 0, html);
        SetError(error, errorCapacity, L"Не удалось прочитать HTML источника.");
        return false;
    }

    bool ok = ExtractTextBetween(visible, L"Album name:", L"Song name:",
            outTrack->album, TRACK_ALBUM_CAPACITY)
        && ExtractTextBetween(visible, L"Song name:", L"Click here",
            outTrack->title, TRACK_TITLE_CAPACITY)
        && FindAudioLink(html, finalPageUrl,
            outTrack->audioUrl, TRACK_URL_CAPACITY);
    if (!ok)
    {
        SetError(error, errorCapacity,
            L"Формат страницы Khinsider изменился или трек недоступен.");
        HeapFree(GetProcessHeap(), 0, visible);
        HeapFree(GetProcessHeap(), 0, html);
        return false;
    }
    FindArtwork(html, finalPageUrl,
        outTrack->artworkUrl, TRACK_URL_CAPACITY);
    wcsncpy_s(outTrack->pageUrl, TRACK_URL_CAPACITY,
        finalPageUrl, _TRUNCATE);
    outTrack->id = TrackHashText(outTrack->audioUrl);
    outTrack->local = false;
    SetError(error, errorCapacity, L"");
    HeapFree(GetProcessHeap(), 0, visible);
    HeapFree(GetProcessHeap(), 0, html);
    return true;
}
