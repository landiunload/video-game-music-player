#include "player_state.h"

#include <windows.h>
#include <stdint.h>
#include <string.h>

#define STATE_MAGIC UINT32_C(0x5249554c)
#define STATE_VERSION UINT32_C(1)

typedef struct StateFileHeader
{
    uint32_t magic;
    uint32_t version;
    uint32_t trackSize;
    uint32_t historyCount;
    int32_t historyCursor;
    float volume;
    uint32_t muted;
    uint32_t reserved;
} StateFileHeader;

static bool StatePath(wchar_t* output, size_t capacity, bool temporary)
{
    wchar_t root[TRACK_PATH_CAPACITY];
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", root,
        TRACK_PATH_CAPACITY);
    if (length == 0 || length >= TRACK_PATH_CAPACITY)
        length = GetTempPathW(TRACK_PATH_CAPACITY, root);
    if (length == 0 || length >= TRACK_PATH_CAPACITY) return false;
    wchar_t directory[TRACK_PATH_CAPACITY];
    _snwprintf_s(directory, TRACK_PATH_CAPACITY, _TRUNCATE,
        L"%s\\laiue-radio", root);
    CreateDirectoryW(directory, NULL);
    _snwprintf_s(output, capacity, _TRUNCATE, L"%s\\state.bin%s",
        directory, temporary ? L".part" : L"");
    return true;
}

void PlayerStateDefaults(PlayerState* state)
{
    memset(state, 0, sizeof(*state));
    state->volume = 0.72f;
    state->historyCursor = -1;
}

static bool TrackIsTerminated(const Track* track)
{
    return wmemchr(track->title, L'\0', TRACK_TITLE_CAPACITY) != NULL
        && wmemchr(track->album, L'\0', TRACK_ALBUM_CAPACITY) != NULL
        && wmemchr(track->pageUrl, L'\0', TRACK_URL_CAPACITY) != NULL
        && wmemchr(track->audioUrl, L'\0', TRACK_URL_CAPACITY) != NULL
        && wmemchr(track->artworkUrl, L'\0', TRACK_URL_CAPACITY) != NULL
        && wmemchr(track->audioPath, L'\0', TRACK_PATH_CAPACITY) != NULL
        && wmemchr(track->artworkPath, L'\0', TRACK_PATH_CAPACITY) != NULL;
}

bool PlayerStateLoad(PlayerState* state)
{
    if (state == NULL) return false;
    PlayerStateDefaults(state);
    wchar_t path[TRACK_PATH_CAPACITY];
    if (!StatePath(path, TRACK_PATH_CAPACITY, false)) return false;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    StateFileHeader header;
    DWORD read = 0;
    bool ok = ReadFile(file, &header, sizeof(header), &read, NULL)
        && read == sizeof(header)
        && header.magic == STATE_MAGIC && header.version == STATE_VERSION
        && header.trackSize == sizeof(Track)
        && header.historyCount <= PLAYER_HISTORY_CAPACITY;
    if (ok && header.historyCount > 0)
    {
        DWORD bytes = header.historyCount * (DWORD)sizeof(Track);
        ok = ReadFile(file, state->history, bytes, &read, NULL)
            && read == bytes;
        for (uint32_t i = 0; ok && i < header.historyCount; ++i)
            ok = TrackIsTerminated(&state->history[i]);
    }
    CloseHandle(file);
    if (!ok)
    {
        PlayerStateDefaults(state);
        return false;
    }
    state->historyCount = header.historyCount;
    state->historyCursor = header.historyCursor;
    if (state->historyCursor < -1
        || state->historyCursor >= (int32_t)state->historyCount)
        state->historyCursor = (int32_t)state->historyCount - 1;
    state->volume = header.volume;
    if (!(state->volume >= 0.0f && state->volume <= 1.0f))
        state->volume = 0.72f;
    state->muted = header.muted != 0;
    return true;
}

bool PlayerStateSave(const PlayerState* state)
{
    if (state == NULL || state->historyCount > PLAYER_HISTORY_CAPACITY)
        return false;
    wchar_t path[TRACK_PATH_CAPACITY];
    wchar_t temporary[TRACK_PATH_CAPACITY];
    if (!StatePath(path, TRACK_PATH_CAPACITY, false)
        || !StatePath(temporary, TRACK_PATH_CAPACITY, true)) return false;
    HANDLE file = CreateFileW(temporary, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    StateFileHeader header = {
        .magic = STATE_MAGIC,
        .version = STATE_VERSION,
        .trackSize = sizeof(Track),
        .historyCount = (uint32_t)state->historyCount,
        .historyCursor = state->historyCursor,
        .volume = state->volume,
        .muted = state->muted ? 1u : 0u,
    };
    DWORD written = 0;
    bool ok = WriteFile(file, &header, sizeof(header), &written, NULL)
        && written == sizeof(header);
    if (ok && state->historyCount > 0)
    {
        DWORD bytes = (DWORD)state->historyCount * (DWORD)sizeof(Track);
        ok = WriteFile(file, state->history, bytes, &written, NULL)
            && written == bytes;
    }
    if (ok) ok = FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    if (ok) ok = MoveFileExW(temporary, path,
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!ok) DeleteFileW(temporary);
    return ok;
}

void PlayerStateRecord(PlayerState* state, const Track* track)
{
    if (state == NULL || track == NULL) return;
    if (state->historyCursor >= 0
        && state->historyCursor < (int32_t)state->historyCount
        && state->history[state->historyCursor].id == track->id)
        return;
    size_t keep = state->historyCursor >= 0
        ? (size_t)state->historyCursor + 1 : 0;
    if (keep < state->historyCount) state->historyCount = keep;
    if (state->historyCount == PLAYER_HISTORY_CAPACITY)
    {
        memmove(state->history, state->history + 1,
            (PLAYER_HISTORY_CAPACITY - 1) * sizeof(Track));
        --state->historyCount;
    }
    state->history[state->historyCount++] = *track;
    state->historyCursor = (int32_t)state->historyCount - 1;
}

