#define COBJMACROS
#include "source/radio_source.h"
#include "source/ocremix.h"

#include <windows.h>
#include <winhttp.h>
#include <shlwapi.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SOURCE_EVENT_CAPACITY 4
#define SOURCE_HTML_LIMIT (4u * 1024u * 1024u)
#define SOURCE_AUDIO_LIMIT (256ull * 1024ull * 1024ull)
#define SOURCE_ARTWORK_LIMIT (16ull * 1024ull * 1024ull)
#define OCREMIX_MAX_ID 5046u

typedef struct HttpRequest
{
    HINTERNET connection;
    HINTERNET request;
} HttpRequest;

struct RadioSource
{
    HANDLE thread;
    HANDLE wakeEvent;
    CRITICAL_SECTION lock;
    HINTERNET session;
    volatile LONG stop;
    volatile LONG requested;
    volatile LONG busy;
    volatile LONG phase;
    volatile LONG progressPermille;
    RadioSourceEvent events[SOURCE_EVENT_CAPACITY];
    uint32_t eventRead;
    uint32_t eventCount;
    uint64_t randomState;
    wchar_t cacheDirectory[TRACK_PATH_CAPACITY];
};

static void CloseHttpRequest(HttpRequest* request)
{
    if (request->request != NULL) WinHttpCloseHandle(request->request);
    if (request->connection != NULL) WinHttpCloseHandle(request->connection);
    memset(request, 0, sizeof(*request));
}

static bool OpenHttpRequest(RadioSource* source, const wchar_t* url,
    const wchar_t* referer, HttpRequest* outRequest,
    wchar_t* error, size_t errorCapacity)
{
    memset(outRequest, 0, sizeof(*outRequest));
    URL_COMPONENTS parts = { .dwStructSize = sizeof(parts) };
    parts.dwHostNameLength = (DWORD)-1;
    parts.dwUrlPathLength = (DWORD)-1;
    parts.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(url, 0, 0, &parts)
        || (parts.nScheme != INTERNET_SCHEME_HTTP
            && parts.nScheme != INTERNET_SCHEME_HTTPS))
    {
        wcsncpy_s(error, errorCapacity, L"Источник вернул некорректный URL.", _TRUNCATE);
        return false;
    }
    wchar_t host[256];
    if (parts.dwHostNameLength == 0 || parts.dwHostNameLength >= 256)
    {
        wcsncpy_s(error, errorCapacity, L"Слишком длинное имя сервера.", _TRUNCATE);
        return false;
    }
    wmemcpy(host, parts.lpszHostName, parts.dwHostNameLength);
    host[parts.dwHostNameLength] = L'\0';

    wchar_t object[TRACK_URL_CAPACITY];
    size_t pathLength = parts.dwUrlPathLength;
    size_t extraLength = parts.dwExtraInfoLength == (DWORD)-1
        ? 0 : parts.dwExtraInfoLength;
    if (pathLength + extraLength + 1 >= TRACK_URL_CAPACITY)
    {
        wcsncpy_s(error, errorCapacity, L"Слишком длинный адрес источника.", _TRUNCATE);
        return false;
    }
    if (pathLength > 0) wmemcpy(object, parts.lpszUrlPath, pathLength);
    else { object[0] = L'/'; pathLength = 1; }
    if (extraLength > 0)
        wmemcpy(object + pathLength, parts.lpszExtraInfo, extraLength);
    object[pathLength + extraLength] = L'\0';

    outRequest->connection = WinHttpConnect(source->session, host,
        parts.nPort, 0);
    if (outRequest->connection == NULL) goto failed;
    DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS
        ? WINHTTP_FLAG_SECURE : 0;
    outRequest->request = WinHttpOpenRequest(outRequest->connection,
        L"GET", object, NULL, referer, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (outRequest->request == NULL) goto failed;

    const wchar_t* headers =
        L"Accept: text/html,application/xhtml+xml,application/octet-stream;q=0.9,*/*;q=0.8\r\n"
        L"Accept-Language: en-US,en;q=0.8\r\n";
    if (!WinHttpAddRequestHeaders(outRequest->request, headers, (DWORD)-1,
            WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE)
        || !WinHttpSendRequest(outRequest->request,
            WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        || !WinHttpReceiveResponse(outRequest->request, NULL))
        goto failed;

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(outRequest->request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
            WINHTTP_NO_HEADER_INDEX)
        || status < 200 || status >= 300)
    {
        _snwprintf_s(error, errorCapacity, _TRUNCATE,
            L"Источник ответил кодом HTTP %lu.", status);
        CloseHttpRequest(outRequest);
        return false;
    }
    return true;

failed:
    _snwprintf_s(error, errorCapacity, _TRUNCATE,
        L"Ошибка сети WinHTTP: %lu.", GetLastError());
    CloseHttpRequest(outRequest);
    return false;
}

static bool ReadHttpMemory(RadioSource* source, const wchar_t* url,
    uint32_t limit, uint8_t** outBytes, size_t* outSize,
    wchar_t* outFinalUrl, size_t finalUrlCapacity,
    wchar_t* error, size_t errorCapacity)
{
    *outBytes = NULL;
    *outSize = 0;
    HttpRequest request;
    if (!OpenHttpRequest(source, url, NULL, &request, error, errorCapacity))
        return false;

    uint32_t capacity = 64u * 1024u;
    uint8_t* bytes = HeapAlloc(GetProcessHeap(), 0, capacity + 1u);
    size_t size = 0;
    bool succeeded = bytes != NULL;
    while (succeeded && InterlockedCompareExchange(&source->stop, 0, 0) == 0)
    {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.request, &available))
        {
            succeeded = false;
            break;
        }
        if (available == 0) break;
        if (size + available > limit)
        {
            wcsncpy_s(error, errorCapacity,
                L"Страница источника превышает допустимый размер.", _TRUNCATE);
            succeeded = false;
            break;
        }
        if (size + available > capacity)
        {
            uint32_t next = capacity;
            while (next < size + available) next *= 2u;
            if (next > limit) next = limit;
            uint8_t* grown = HeapReAlloc(GetProcessHeap(), 0, bytes,
                (size_t)next + 1u);
            if (grown == NULL) { succeeded = false; break; }
            bytes = grown;
            capacity = next;
        }
        DWORD read = 0;
        if (!WinHttpReadData(request.request, bytes + size, available, &read))
        {
            succeeded = false;
            break;
        }
        size += read;
    }
    if (InterlockedCompareExchange(&source->stop, 0, 0) != 0)
        succeeded = false;
    if (succeeded)
    {
        bytes[size] = 0;
        DWORD urlBytes = (DWORD)(finalUrlCapacity * sizeof(wchar_t));
        if (!WinHttpQueryOption(request.request, WINHTTP_OPTION_URL,
            outFinalUrl, &urlBytes))
            wcsncpy_s(outFinalUrl, finalUrlCapacity, url, _TRUNCATE);
        *outBytes = bytes;
        *outSize = size;
        bytes = NULL;
    }
    else if (error[0] == L'\0')
    {
        _snwprintf_s(error, errorCapacity, _TRUNCATE,
            L"Не удалось прочитать ответ источника: %lu.", GetLastError());
    }
    if (bytes != NULL) HeapFree(GetProcessHeap(), 0, bytes);
    CloseHttpRequest(&request);
    return succeeded;
}

static uint64_t QueryContentLength(HINTERNET request)
{
    wchar_t value[64];
    DWORD bytes = sizeof(value);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH,
        WINHTTP_HEADER_NAME_BY_INDEX, value, &bytes,
        WINHTTP_NO_HEADER_INDEX)) return 0;
    return _wcstoui64(value, NULL, 10);
}

static bool DownloadFile(RadioSource* source, const wchar_t* url,
    const wchar_t* referer, const wchar_t* target, uint64_t limit,
    bool reportProgress, wchar_t* error, size_t errorCapacity)
{
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    if (GetFileAttributesExW(target, GetFileExInfoStandard, &attributes))
    {
        uint64_t size = ((uint64_t)attributes.nFileSizeHigh << 32)
            | attributes.nFileSizeLow;
        if (size > 0) return true;
    }

    wchar_t temporary[TRACK_PATH_CAPACITY];
    _snwprintf_s(temporary, TRACK_PATH_CAPACITY, _TRUNCATE,
        L"%s.part", target);
    DeleteFileW(temporary);
    HANDLE file = CreateFileW(temporary, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        wcsncpy_s(error, errorCapacity, L"Не удалось создать файл кэша.", _TRUNCATE);
        return false;
    }
    HttpRequest request;
    bool succeeded = OpenHttpRequest(source, url, referer,
        &request, error, errorCapacity);
    uint64_t expected = succeeded ? QueryContentLength(request.request) : 0;
    if (expected > limit)
    {
        wcsncpy_s(error, errorCapacity, L"Трек слишком большой для кэша.", _TRUNCATE);
        succeeded = false;
    }
    uint64_t total = 0;
    uint8_t buffer[64u * 1024u];
    while (succeeded && InterlockedCompareExchange(&source->stop, 0, 0) == 0)
    {
        DWORD read = 0;
        if (!WinHttpReadData(request.request, buffer, sizeof(buffer), &read))
        {
            succeeded = false;
            break;
        }
        if (read == 0) break;
        total += read;
        if (total > limit)
        {
            wcsncpy_s(error, errorCapacity, L"Трек слишком большой для кэша.", _TRUNCATE);
            succeeded = false;
            break;
        }
        DWORD written = 0;
        if (!WriteFile(file, buffer, read, &written, NULL) || written != read)
        {
            wcsncpy_s(error, errorCapacity, L"Ошибка записи в кэш.", _TRUNCATE);
            succeeded = false;
            break;
        }
        if (reportProgress && expected > 0)
        {
            LONG progress = (LONG)((total * 1000u) / expected);
            if (progress > 1000) progress = 1000;
            InterlockedExchange(&source->progressPermille, progress);
        }
    }
    if (InterlockedCompareExchange(&source->stop, 0, 0) != 0)
        succeeded = false;
    CloseHandle(file);
    if (request.request != NULL || request.connection != NULL)
        CloseHttpRequest(&request);
    if (succeeded && total > 0)
    {
        succeeded = MoveFileExW(temporary, target,
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
        if (!succeeded)
            wcsncpy_s(error, errorCapacity, L"Не удалось завершить файл кэша.", _TRUNCATE);
    }
    else if (succeeded)
    {
        wcsncpy_s(error, errorCapacity, L"Источник вернул пустой файл.", _TRUNCATE);
        succeeded = false;
    }
    if (!succeeded) DeleteFileW(temporary);
    return succeeded;
}

static void PushEvent(RadioSource* source, const RadioSourceEvent* event)
{
    EnterCriticalSection(&source->lock);
    if (source->eventCount == SOURCE_EVENT_CAPACITY)
    {
        source->eventRead = (source->eventRead + 1) % SOURCE_EVENT_CAPACITY;
        --source->eventCount;
    }
    uint32_t write = (source->eventRead + source->eventCount)
        % SOURCE_EVENT_CAPACITY;
    source->events[write] = *event;
    ++source->eventCount;
    LeaveCriticalSection(&source->lock);
}

static const wchar_t* AudioExtension(const wchar_t* url)
{
    const wchar_t* question = wcschr(url, L'?');
    const wchar_t* dot = wcsrchr(url, L'.');
    if (dot == NULL || (question != NULL && dot > question)) return L".audio";
    if (_wcsnicmp(dot, L".mp3", 4) == 0) return L".mp3";
    if (_wcsnicmp(dot, L".ogg", 4) == 0) return L".ogg";
    if (_wcsnicmp(dot, L".m4a", 4) == 0) return L".m4a";
    return L".audio";
}

static uint32_t NextRandom(RadioSource* source)
{
    uint64_t value = source->randomState;
    value ^= value >> 12;
    value ^= value << 25;
    value ^= value >> 27;
    source->randomState = value;
    return (uint32_t)((value * UINT64_C(2685821657736338717)) >> 32);
}

static bool IsExpectedTrackPage(const wchar_t* url)
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
    return _wcsicmp(host, L"ocremix.org") == 0
        && StrStrIW(path, L"/remix/OCR") != NULL;
}

static void DoRandomRequest(RadioSource* source)
{
    RadioSourceEvent event = { .type = RADIO_SOURCE_EVENT_ERROR };
    wchar_t finalUrl[TRACK_URL_CAPACITY] = L"";
    uint8_t* html = NULL;
    size_t htmlSize = 0;
    InterlockedExchange(&source->phase, RADIO_SOURCE_FINDING);
    InterlockedExchange(&source->progressPermille, 0);
    bool parsed = false;
    for (int attempt = 0; attempt < 8 && !parsed; ++attempt)
    {
        if (html != NULL)
        {
            HeapFree(GetProcessHeap(), 0, html);
            html = NULL;
        }
        uint32_t remixId = NextRandom(source) % OCREMIX_MAX_ID + 1u;
        wchar_t requestUrl[128];
        _snwprintf_s(requestUrl, 128, _TRUNCATE,
            L"https://ocremix.org/remix/OCR%05u/", remixId);
        event.message[0] = L'\0';
        bool received = ReadHttpMemory(source, requestUrl,
            SOURCE_HTML_LIMIT, &html, &htmlSize,
            finalUrl, TRACK_URL_CAPACITY,
            event.message, sizeof(event.message) / sizeof(event.message[0]));
        parsed = received && IsExpectedTrackPage(finalUrl)
            && OcremixParseTrackPage((const char*)html, htmlSize, finalUrl,
                &event.track, event.message,
                sizeof(event.message) / sizeof(event.message[0]));
    }
    if (!parsed)
        goto done;

    _snwprintf_s(event.track.audioPath, TRACK_PATH_CAPACITY, _TRUNCATE,
        L"%s\\%016llx%s", source->cacheDirectory,
        (unsigned long long)event.track.id, AudioExtension(event.track.audioUrl));
    InterlockedExchange(&source->phase, RADIO_SOURCE_DOWNLOADING_AUDIO);
    if (!DownloadFile(source, event.track.audioUrl, event.track.pageUrl,
        event.track.audioPath, SOURCE_AUDIO_LIMIT, true,
        event.message, sizeof(event.message) / sizeof(event.message[0])))
        goto done;

    if (event.track.artworkUrl[0] != L'\0')
    {
        _snwprintf_s(event.track.artworkPath, TRACK_PATH_CAPACITY, _TRUNCATE,
            L"%s\\%016llx.cover", source->cacheDirectory,
            (unsigned long long)event.track.id);
        InterlockedExchange(&source->phase, RADIO_SOURCE_DOWNLOADING_ARTWORK);
        if (!DownloadFile(source, event.track.artworkUrl, event.track.pageUrl,
            event.track.artworkPath, SOURCE_ARTWORK_LIMIT, false,
            event.message, sizeof(event.message) / sizeof(event.message[0])))
        {
            // Обложка необязательна: воспроизведение не должно ломаться.
            event.track.artworkPath[0] = L'\0';
        }
    }
    event.type = RADIO_SOURCE_EVENT_TRACK_READY;
    event.message[0] = L'\0';

done:
    if (html != NULL) HeapFree(GetProcessHeap(), 0, html);
    if (InterlockedCompareExchange(&source->stop, 0, 0) == 0)
        PushEvent(source, &event);
    InterlockedExchange(&source->phase, RADIO_SOURCE_IDLE);
    InterlockedExchange(&source->progressPermille, 0);
}

static DWORD WINAPI SourceThread(void* parameter)
{
    RadioSource* source = parameter;
    while (InterlockedCompareExchange(&source->stop, 0, 0) == 0)
    {
        WaitForSingleObject(source->wakeEvent, INFINITE);
        if (InterlockedCompareExchange(&source->stop, 0, 0) != 0) break;
        if (InterlockedExchange(&source->requested, 0) != 0)
        {
            InterlockedExchange(&source->busy, 1);
            DoRandomRequest(source);
            InterlockedExchange(&source->busy, 0);
        }
    }
    return 0;
}

RadioSource* RadioSourceCreate(void)
{
    RadioSource* source = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
        sizeof(*source));
    if (source == NULL) return NULL;
    InitializeCriticalSection(&source->lock);
    source->wakeEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    source->session = WinHttpOpen(
        L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) laiue-radio/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    wchar_t root[TRACK_PATH_CAPACITY];
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", root,
        TRACK_PATH_CAPACITY);
    if (length == 0 || length >= TRACK_PATH_CAPACITY)
        GetTempPathW(TRACK_PATH_CAPACITY, root);
    _snwprintf_s(source->cacheDirectory, TRACK_PATH_CAPACITY, _TRUNCATE,
        L"%s\\laiue-radio\\cache", root);
    wchar_t parent[TRACK_PATH_CAPACITY];
    _snwprintf_s(parent, TRACK_PATH_CAPACITY, _TRUNCATE,
        L"%s\\laiue-radio", root);
    CreateDirectoryW(parent, NULL);
    CreateDirectoryW(source->cacheDirectory, NULL);
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    source->randomState = (uint64_t)counter.QuadPart
        ^ GetTickCount64() ^ (uint64_t)(uintptr_t)source;
    if (source->randomState == 0) source->randomState = UINT64_C(1);
    if (source->wakeEvent == NULL || source->session == NULL)
        goto failed;
    WinHttpSetTimeouts(source->session, 5000, 10000, 15000, 15000);
    source->thread = CreateThread(NULL, 0, SourceThread, source, 0, NULL);
    if (source->thread == NULL) goto failed;
    return source;

failed:
    if (source->session != NULL) WinHttpCloseHandle(source->session);
    if (source->wakeEvent != NULL) CloseHandle(source->wakeEvent);
    DeleteCriticalSection(&source->lock);
    HeapFree(GetProcessHeap(), 0, source);
    return NULL;
}

void RadioSourceDestroy(RadioSource* source)
{
    if (source == NULL) return;
    InterlockedExchange(&source->stop, 1);
    SetEvent(source->wakeEvent);
    // Закрытие родительского WinHTTP handle прерывает синхронный I/O,
    // поэтому выход не ждёт сетевого таймаута.
    WinHttpCloseHandle(source->session);
    source->session = NULL;
    WaitForSingleObject(source->thread, INFINITE);
    CloseHandle(source->thread);
    CloseHandle(source->wakeEvent);
    DeleteCriticalSection(&source->lock);
    HeapFree(GetProcessHeap(), 0, source);
}

bool RadioSourceRequestRandom(RadioSource* source)
{
    if (source == NULL || InterlockedCompareExchange(&source->busy, 0, 0) != 0
        || InterlockedCompareExchange(&source->requested, 1, 0) != 0)
        return false;
    SetEvent(source->wakeEvent);
    return true;
}

bool RadioSourcePollEvent(RadioSource* source, RadioSourceEvent* outEvent)
{
    if (source == NULL || outEvent == NULL) return false;
    bool available = false;
    EnterCriticalSection(&source->lock);
    if (source->eventCount > 0)
    {
        *outEvent = source->events[source->eventRead];
        source->eventRead = (source->eventRead + 1) % SOURCE_EVENT_CAPACITY;
        --source->eventCount;
        available = true;
    }
    LeaveCriticalSection(&source->lock);
    return available;
}

RadioSourcePhase RadioSourceGetPhase(const RadioSource* source)
{
    return source == NULL ? RADIO_SOURCE_IDLE
        : (RadioSourcePhase)InterlockedCompareExchange(
            (volatile LONG*)&source->phase, 0, 0);
}

int32_t RadioSourceGetProgressPermille(const RadioSource* source)
{
    return source == NULL ? 0 : InterlockedCompareExchange(
        (volatile LONG*)&source->progressPermille, 0, 0);
}

const wchar_t* RadioSourceGetCacheDirectory(const RadioSource* source)
{
    return source == NULL ? L"" : source->cacheDirectory;
}
