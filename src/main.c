#include "engine/audio.h"
#include "engine/input.h"
#include "engine/time.h"
#include "engine/ui.h"
#include "engine/ui_renderer.h"
#include "engine/window.h"
#include "player_state.h"
#include "source/radio_source.h"
#include "track.h"

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define RADIO_QUEUE_CAPACITY 6

typedef enum QueueTab
{
    QUEUE_TAB_UPCOMING = 0,
    QUEUE_TAB_HISTORY = 1,
} QueueTab;

typedef struct Application
{
    Window* window;
    Input* input;
    UiRenderer* renderer;
    AudioPlayer* audio;
    RadioSource* source;
    UiContext ui;
    PlayerState saved;
    Track current;
    bool hasCurrent;
    bool autoplayAfterLoad;
    bool imageLoaded;
    uint32_t imageWidth;
    uint32_t imageHeight;
    Track queue[RADIO_QUEUE_CAPACITY];
    size_t queueCount;
    QueueTab queueTab;
    bool onlinePaused;
    wchar_t message[384];
    double messageUntil;
    int32_t width;
    int32_t height;
    double previousTime;
} Application;

static void SetMessage(Application* application, const wchar_t* message,
    double seconds)
{
    wcsncpy_s(application->message,
        sizeof(application->message) / sizeof(application->message[0]),
        message, _TRUNCATE);
    application->messageUntil = seconds > 0.0
        ? PlatformTimeSeconds() + seconds : 0.0;
}

static void FormatTime(double seconds, wchar_t* output, size_t capacity)
{
    if (!(seconds >= 0.0) || seconds > 359999.0) seconds = 0.0;
    uint32_t total = (uint32_t)seconds;
    uint32_t minutes = total / 60u;
    uint32_t remainder = total % 60u;
    _snwprintf_s(output, capacity, _TRUNCATE,
        L"%u:%02u", minutes, remainder);
}

static void Ellipsize(const wchar_t* source, wchar_t* output,
    size_t capacity, size_t maximumCharacters)
{
    if (capacity == 0) return;
    if (source == NULL) source = L"";
    size_t length = wcslen(source);
    if (length <= maximumCharacters)
    {
        wcsncpy_s(output, capacity, source, _TRUNCATE);
        return;
    }
    size_t take = maximumCharacters > 3 ? maximumCharacters - 3 : 0;
    if (take >= capacity) take = capacity - 1;
    wmemcpy(output, source, take);
    output[take] = L'\0';
    if (capacity - take > 3) wcscat_s(output, capacity, L"...");
}

static bool TrackFileExists(const Track* track)
{
    DWORD attributes = GetFileAttributesW(track->audioPath);
    return track->audioPath[0] != L'\0'
        && attributes != INVALID_FILE_ATTRIBUTES
        && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static void LoadCover(Application* application, const Track* track)
{
    application->imageLoaded = false;
    application->imageWidth = 0;
    application->imageHeight = 0;
    UiRendererClearImage(application->renderer);
    if (track->artworkPath[0] != L'\0')
    {
        application->imageLoaded = UiRendererLoadImage(application->renderer,
            track->artworkPath, &application->imageWidth,
            &application->imageHeight);
    }
}

static bool PlayTrack(Application* application, const Track* track,
    bool record, bool autoplay)
{
    if (track == NULL || !TrackFileExists(track))
    {
        SetMessage(application, L"Файл трека больше недоступен.", 4.0);
        return false;
    }
    if (AudioPlayerLoadFile(application->audio, track->audioPath)
        != AUDIO_RESULT_OK)
    {
        SetMessage(application, L"Media Foundation не принял этот файл.", 4.0);
        return false;
    }
    application->current = *track;
    application->hasCurrent = true;
    application->autoplayAfterLoad = autoplay;
    if (record)
        PlayerStateRecord(&application->saved, track);
    LoadCover(application, track);
    PlayerStateSave(&application->saved);
    return true;
}

static void RequestOnlineQueue(Application* application)
{
    if (application->source == NULL || application->onlinePaused
        || application->queueCount >= 3) return;
    RadioSourceRequestRandom(application->source);
}

static void NextTrack(Application* application)
{
    if (application->saved.historyCursor + 1
        < (int32_t)application->saved.historyCount)
    {
        ++application->saved.historyCursor;
        PlayTrack(application,
            &application->saved.history[application->saved.historyCursor],
            false, true);
        return;
    }
    if (application->queueCount == 0)
    {
        application->onlinePaused = false;
        RequestOnlineQueue(application);
        SetMessage(application, L"Готовим следующий случайный трек...", 3.0);
        return;
    }
    Track next = application->queue[0];
    --application->queueCount;
    if (application->queueCount > 0)
        memmove(application->queue, application->queue + 1,
            application->queueCount * sizeof(Track));
    PlayTrack(application, &next, true, true);
    RequestOnlineQueue(application);
}

static void PreviousTrack(Application* application,
    const AudioPlayerSnapshot* snapshot)
{
    if (snapshot->positionSeconds > 5.0)
    {
        AudioPlayerSeek(application->audio, 0.0);
        return;
    }
    if (application->saved.historyCursor <= 0)
    {
        AudioPlayerSeek(application->audio, 0.0);
        return;
    }
    --application->saved.historyCursor;
    PlayTrack(application,
        &application->saved.history[application->saved.historyCursor],
        false, true);
}

static void TogglePlayback(Application* application,
    const AudioPlayerSnapshot* snapshot)
{
    if (!application->hasCurrent)
    {
        application->onlinePaused = false;
        RequestOnlineQueue(application);
        SetMessage(application, L"Ищем музыку...", 3.0);
        return;
    }
    if (snapshot->state == AUDIO_PLAYBACK_PLAYING
        || snapshot->state == AUDIO_PLAYBACK_BUFFERING)
        AudioPlayerPause(application->audio);
    else if (snapshot->state != AUDIO_PLAYBACK_LOADING)
        AudioPlayerPlay(application->audio);
}

static void ToggleMuted(Application* application)
{
    application->saved.muted = !application->saved.muted;
    AudioPlayerSetMuted(application->audio, application->saved.muted);
    PlayerStateSave(&application->saved);
}

static bool OpenLocalTrack(Application* application)
{
    wchar_t path[TRACK_PATH_CAPACITY] = L"";
    OPENFILENAMEW dialog = {
        .lStructSize = sizeof(dialog),
        .hwndOwner = (HWND)WindowGetNativeHandle(application->window),
        .lpstrFilter = L"Аудиофайлы\0*.mp3;*.ogg;*.m4a;*.wav;*.flac;*.wma;*.aac\0Все файлы\0*.*\0",
        .lpstrFile = path,
        .nMaxFile = TRACK_PATH_CAPACITY,
        .lpstrTitle = L"Открыть музыку",
        .Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
            | OFN_HIDEREADONLY | OFN_NOCHANGEDIR,
    };
    if (!GetOpenFileNameW(&dialog)) return false;
    Track track;
    TrackFromLocalFile(&track, path);
    application->onlinePaused = false;
    return PlayTrack(application, &track, true, true);
}

static void PollRadioSource(Application* application)
{
    if (application->source == NULL) return;
    RadioSourceEvent event;
    while (RadioSourcePollEvent(application->source, &event))
    {
        if (event.type == RADIO_SOURCE_EVENT_ERROR)
        {
            application->onlinePaused = true;
            SetMessage(application, event.message[0] != L'\0' ? event.message
                : L"Онлайн-источник временно недоступен.", 0.0);
            continue;
        }
        bool duplicate = application->hasCurrent
            && event.track.id == application->current.id;
        for (size_t i = 0; i < application->queueCount; ++i)
            duplicate = duplicate || application->queue[i].id == event.track.id;
        if (!duplicate && !application->hasCurrent)
        {
            PlayTrack(application, &event.track, true, true);
        }
        else if (!duplicate && application->queueCount < RADIO_QUEUE_CAPACITY)
        {
            application->queue[application->queueCount++] = event.track;
        }
    }
    RequestOnlineQueue(application);
}

static void PollAudio(Application* application)
{
    AudioEvent event;
    while (AudioPlayerPollEvent(application->audio, &event))
    {
        if (event.type == AUDIO_EVENT_READY && application->autoplayAfterLoad)
        {
            application->autoplayAfterLoad = false;
            AudioPlayerPlay(application->audio);
        }
        else if (event.type == AUDIO_EVENT_ENDED)
        {
            NextTrack(application);
        }
        else if (event.type == AUDIO_EVENT_ERROR)
        {
            application->autoplayAfterLoad = false;
            wchar_t text[384];
            _snwprintf_s(text, 384, _TRUNCATE,
                L"Ошибка декодирования Media Foundation (0x%08x).",
                (uint32_t)event.platformCode);
            SetMessage(application, text, 0.0);
        }
    }
}

static void DrawCover(Application* application, float x, float y, float size)
{
    UiContext* ui = &application->ui;
    UiRect(ui, x - UiScaled(ui, 9.0f), y - UiScaled(ui, 5.0f),
        size + UiScaled(ui, 18.0f), size + UiScaled(ui, 18.0f),
        UiScaled(ui, 24.0f), UiColor(0, 0, 0, 78));
    if (application->imageLoaded)
    {
        float sourceAspect = application->imageHeight > 0
            ? (float)application->imageWidth / (float)application->imageHeight
            : 1.0f;
        float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
        if (sourceAspect > 1.0f)
        {
            float span = 1.0f / sourceAspect;
            u0 = (1.0f - span) * 0.5f;
            u1 = u0 + span;
        }
        else if (sourceAspect > 0.0f && sourceAspect < 1.0f)
        {
            float span = sourceAspect;
            v0 = (1.0f - span) * 0.5f;
            v1 = v0 + span;
        }
        UiImage(ui, x, y, size, size, u0, v0, u1, v1,
            UiColor(255, 255, 255, 255));
    }
    else
    {
        UiRect(ui, x, y, size, size, UiScaled(ui, 19.0f),
            UiColor(29, 34, 37, 255));
        UiRect(ui, x + size * 0.25f, y + size * 0.25f,
            size * 0.5f, size * 0.5f, size * 0.25f,
            UiColor(201, 255, 104, 38));
        UiTextCentered(ui, x + size * 0.5f,
            y + size * 0.5f - ui->font.lineHeight * 0.5f,
            UiColor(201, 255, 104, 255), L"laiue");
    }
}

static void DrawSidebar(Application* application, float x, float y,
    float width, float height)
{
    UiContext* ui = &application->ui;
    float s = ui->scale;
    UiPanel(ui, x, y, width, height);
    UiRect(ui, x + 18 * s, y + 18 * s, 34 * s, 34 * s, 11 * s,
        UiColor(201, 255, 104, 255));
    UiText(ui, x + 64 * s, y + 17 * s, UI_COLOR_TEXT, L"laiue radio");
    UiText(ui, x + 64 * s, y + 38 * s, UI_COLOR_TEXT_DIM,
        L"D3D12 + Media Foundation");

    float buttonY = y + 86 * s;
    if (UiButton(ui, 100, x + 16 * s, buttonY,
        width - 32 * s, 42 * s, L"OC ReMix радио"))
    {
        application->onlinePaused = false;
        RequestOnlineQueue(application);
        SetMessage(application, L"Ищем случайный саундтрек...", 3.0);
    }
    if (UiButton(ui, 101, x + 16 * s, buttonY + 52 * s,
        width - 32 * s, 42 * s, L"Открыть файл"))
        OpenLocalTrack(application);

    float infoY = y + height - 116 * s;
    UiText(ui, x + 18 * s, infoY, UI_COLOR_TEXT_DIM, L"Горячие клавиши");
    UiText(ui, x + 18 * s, infoY + 26 * s, UI_COLOR_TEXT, L"Space   Play / Pause");
    UiText(ui, x + 18 * s, infoY + 48 * s, UI_COLOR_TEXT, L"← →     Previous / Next");
    UiText(ui, x + 18 * s, infoY + 70 * s, UI_COLOR_TEXT, L"M        Mute");
}

static const wchar_t* SourcePhaseText(RadioSourcePhase phase)
{
    switch (phase)
    {
        case RADIO_SOURCE_FINDING: return L"Ищем случайный трек";
        case RADIO_SOURCE_DOWNLOADING_AUDIO: return L"Кэшируем следующий трек";
        case RADIO_SOURCE_DOWNLOADING_ARTWORK: return L"Загружаем обложку";
        default: return L"";
    }
}

static void DrawCenter(Application* application,
    const AudioPlayerSnapshot* snapshot, float x, float y,
    float width, float height)
{
    UiContext* ui = &application->ui;
    float s = ui->scale;
    UiPanel(ui, x, y, width, height);

    RadioSourcePhase phase = RadioSourceGetPhase(application->source);
    const wchar_t* phaseText = SourcePhaseText(phase);
    if (phase != RADIO_SOURCE_IDLE)
    {
        UiRect(ui, x + 22 * s, y + 20 * s, 8 * s, 8 * s, 4 * s,
            UiColor(201, 255, 104, 255));
        UiText(ui, x + 40 * s, y + 15 * s, UI_COLOR_TEXT_DIM, phaseText);
        if (phase == RADIO_SOURCE_DOWNLOADING_AUDIO)
        {
            int progress = RadioSourceGetProgressPermille(application->source);
            wchar_t progressText[32];
            _snwprintf_s(progressText, 32, _TRUNCATE, L"%d%%", progress / 10);
            UiText(ui, x + width - 22 * s - UiTextWidth(ui, progressText),
                y + 15 * s, UI_COLOR_TEXT_DIM, progressText);
        }
    }
    else
    {
        UiText(ui, x + 22 * s, y + 15 * s, UI_COLOR_TEXT_DIM,
            application->hasCurrent
                ? (application->current.local ? L"Локальный файл" : L"OverClocked ReMix")
                : L"Музыка не выбрана");
    }
    if (UiButton(ui, 110, x + width - 142 * s, y + 10 * s,
        56 * s, 30 * s, L"Радио"))
    {
        application->onlinePaused = false;
        RequestOnlineQueue(application);
        SetMessage(application, L"Ищем случайный саундтрек...", 3.0);
    }
    if (UiButton(ui, 111, x + width - 78 * s, y + 10 * s,
        56 * s, 30 * s, L"Файл"))
        OpenLocalTrack(application);

    float cover = height * 0.39f;
    float maxCover = width * 0.58f;
    if (cover > maxCover) cover = maxCover;
    if (cover < 124 * s) cover = 124 * s;
    float coverX = x + (width - cover) * 0.5f;
    float coverY = y + 54 * s;
    DrawCover(application, coverX, coverY, cover);

    float titleY = coverY + cover + 25 * s;
    wchar_t title[64];
    wchar_t album[74];
    Ellipsize(application->hasCurrent ? application->current.title
        : L"Открой файл или включи радио", title, 64, 48);
    Ellipsize(application->hasCurrent ? application->current.album
        : L"Музыка из игр — без браузера", album, 74, 60);
    UiTextCentered(ui, x + width * 0.5f, titleY,
        UI_COLOR_TEXT, title);
    UiTextCentered(ui, x + width * 0.5f, titleY + 25 * s,
        UI_COLOR_TEXT_DIM, album);

    float timelineX = x + 42 * s;
    float timelineWidth = width - 84 * s;
    float timelineY = titleY + 67 * s;
    int32_t position = snapshot->durationSeconds > 0.0
        ? (int32_t)(snapshot->positionSeconds
            / snapshot->durationSeconds * 1000.0) : 0;
    if (position < 0) position = 0;
    if (position > 1000) position = 1000;
    if (UiSliderInt(ui, 200, timelineX, timelineY,
        timelineWidth, 0, 1000, &position)
        && snapshot->durationSeconds > 0.0 && snapshot->seekable)
        AudioPlayerSeek(application->audio,
            snapshot->durationSeconds * (double)position / 1000.0);
    wchar_t currentTime[32];
    wchar_t duration[32];
    FormatTime(snapshot->positionSeconds, currentTime, 32);
    FormatTime(snapshot->durationSeconds, duration, 32);
    UiText(ui, timelineX, timelineY + 23 * s,
        UI_COLOR_TEXT_DIM, currentTime);
    UiText(ui, timelineX + timelineWidth - UiTextWidth(ui, duration),
        timelineY + 23 * s, UI_COLOR_TEXT_DIM, duration);

    float controlsY = timelineY + 54 * s;
    float center = x + width * 0.5f;
    if (UiButton(ui, 201, center - 116 * s, controlsY,
        54 * s, 42 * s, L"< Prev"))
        PreviousTrack(application, snapshot);
    bool playing = snapshot->state == AUDIO_PLAYBACK_PLAYING
        || snapshot->state == AUDIO_PLAYBACK_BUFFERING;
    if (UiButton(ui, 202, center - 52 * s, controlsY,
        104 * s, 42 * s, playing ? L"Pause" : L"Play"))
        TogglePlayback(application, snapshot);
    if (UiButton(ui, 203, center + 62 * s, controlsY,
        54 * s, 42 * s, L"Next >"))
        NextTrack(application);

    float volumeY = controlsY + 59 * s;
    if (UiButton(ui, 204, center - 116 * s, volumeY,
        54 * s, 34 * s, application->saved.muted ? L"Muted" : L"Sound"))
        ToggleMuted(application);
    int32_t volume = (int32_t)(application->saved.volume * 100.0f + 0.5f);
    if (UiSliderInt(ui, 205, center - 52 * s, volumeY + 7 * s,
        168 * s, 0, 100, &volume))
    {
        application->saved.volume = (float)volume / 100.0f;
        if (volume > 0 && application->saved.muted)
        {
            application->saved.muted = false;
            AudioPlayerSetMuted(application->audio, false);
        }
        AudioPlayerSetVolume(application->audio, application->saved.volume);
        PlayerStateSave(&application->saved);
    }

    bool visibleMessage = application->message[0] != L'\0'
        && (application->messageUntil == 0.0
            || PlatformTimeSeconds() < application->messageUntil);
    if (visibleMessage)
    {
        wchar_t shortened[96];
        Ellipsize(application->message, shortened, 96, 78);
        UiTextCentered(ui, x + width * 0.5f, y + height - 34 * s,
            UiColor(224, 189, 116, 255), shortened);
    }
}

static void DrawQueueRow(Application* application, const Track* track,
    uint32_t id, int number, float x, float y, float width,
    bool current, int historyIndex)
{
    UiContext* ui = &application->ui;
    float s = ui->scale;
    if (UiButton(ui, id, x, y, width, 52 * s, L""))
    {
        if (historyIndex >= 0)
        {
            application->saved.historyCursor = historyIndex;
            PlayTrack(application, track, false, true);
        }
        else
        {
            size_t index = id - 400u;
            if (index < application->queueCount)
            {
                Track selected = application->queue[index];
                memmove(application->queue + index,
                    application->queue + index + 1,
                    (application->queueCount - index - 1) * sizeof(Track));
                --application->queueCount;
                PlayTrack(application, &selected, true, true);
                RequestOnlineQueue(application);
            }
        }
    }
    wchar_t numberText[8];
    _snwprintf_s(numberText, 8, _TRUNCATE, L"%02d", number);
    UiText(ui, x + 12 * s, y + 4 * s,
        current ? UiColor(201, 255, 104, 255) : UI_COLOR_TEXT_DIM,
        numberText);
    wchar_t title[38];
    wchar_t album[42];
    Ellipsize(track->title, title, 38, 30);
    Ellipsize(track->album, album, 42, 34);
    UiText(ui, x + 48 * s, y + 3 * s,
        current ? UiColor(201, 255, 104, 255) : UI_COLOR_TEXT, title);
    UiText(ui, x + 48 * s, y + 25 * s, UI_COLOR_TEXT_DIM, album);
}

static void DrawQueue(Application* application, float x, float y,
    float width, float height)
{
    UiContext* ui = &application->ui;
    float s = ui->scale;
    UiPanel(ui, x, y, width, height);
    const wchar_t* tabs[] = { L"В очереди", L"История" };
    int32_t tab = application->queueTab;
    if (UiSegmented(ui, 300, x + 16 * s, y + 16 * s,
        width - 32 * s, 36 * s, tabs, 2, &tab))
        application->queueTab = (QueueTab)tab;

    size_t count = application->queueTab == QUEUE_TAB_UPCOMING
        ? application->queueCount : application->saved.historyCount;
    wchar_t countText[48];
    _snwprintf_s(countText, 48, _TRUNCATE, L"%zu треков", count);
    UiText(ui, x + 18 * s, y + 69 * s, UI_COLOR_TEXT_DIM, countText);

    float rowsY = y + 98 * s;
    float rowWidth = width - 28 * s;
    int visibleRows = (int)((height - 114 * s) / (58 * s));
    if (visibleRows < 1) visibleRows = 1;
    if (application->queueTab == QUEUE_TAB_UPCOMING)
    {
        if (application->queueCount == 0)
            UiTextCentered(ui, x + width * 0.5f, rowsY + 26 * s,
                UI_COLOR_TEXT_DIM, L"Очередь готовится");
        for (size_t i = 0; i < application->queueCount
            && (int)i < visibleRows; ++i)
            DrawQueueRow(application, &application->queue[i],
                400u + (uint32_t)i, (int)i + 1,
                x + 14 * s, rowsY + i * 58 * s, rowWidth,
                false, -1);
    }
    else
    {
        if (application->saved.historyCount == 0)
            UiTextCentered(ui, x + width * 0.5f, rowsY + 26 * s,
                UI_COLOR_TEXT_DIM, L"История пока пуста");
        for (int row = 0; row < visibleRows; ++row)
        {
            int index = (int)application->saved.historyCount - 1 - row;
            if (index < 0) break;
            DrawQueueRow(application,
                &application->saved.history[index],
                600u + (uint32_t)row, row + 1,
                x + 14 * s, rowsY + row * 58 * s, rowWidth,
                index == application->saved.historyCursor, index);
        }
    }

    if (application->onlinePaused
        && UiButton(ui, 350, x + 18 * s, y + height - 50 * s,
            width - 36 * s, 34 * s, L"Повторить запрос (R)"))
    {
        application->onlinePaused = false;
        application->message[0] = L'\0';
        RequestOnlineQueue(application);
    }
}

static void DrawPlayer(Application* application,
    const AudioPlayerSnapshot* snapshot)
{
    UiContext* ui = &application->ui;
    float s = ui->scale;
    float margin = 22 * s;
    float gap = 8 * s;
    float contentHeight = (float)application->height - margin * 2;
    bool compact = application->width < 900;
    if (compact)
    {
        DrawCenter(application, snapshot, margin, margin,
            (float)application->width - margin * 2, contentHeight);
        return;
    }
    float sidebar = 216 * s;
    float queue = 292 * s;
    float center = (float)application->width - margin * 2
        - sidebar - queue - gap * 2;
    DrawSidebar(application, margin, margin, sidebar, contentHeight);
    DrawCenter(application, snapshot, margin + sidebar + gap, margin,
        center, contentHeight);
    DrawQueue(application, margin + sidebar + gap + center + gap,
        margin, queue, contentHeight);
}

static void HandleRawInput(void* userData, void* rawInputHandle)
{
    InputHandleRawInput((Input*)userData, rawInputHandle);
}

static void HandleKeyboard(Application* application,
    const AudioPlayerSnapshot* snapshot)
{
    if (InputConsumeKeyPress(application->input, INPUT_KEY_ESCAPE))
        WindowRequestClose(application->window);
    if (InputConsumeKeyPress(application->input, INPUT_KEY_F11))
        WindowSetFullscreen(application->window,
            !WindowIsFullscreen(application->window));
    if (InputConsumeKeyPress(application->input, INPUT_KEY_SPACE))
        TogglePlayback(application, snapshot);
    if (InputConsumeKeyPress(application->input, INPUT_KEY_LEFT))
        PreviousTrack(application, snapshot);
    if (InputConsumeKeyPress(application->input, INPUT_KEY_RIGHT))
        NextTrack(application);
    if (InputConsumeKeyPress(application->input, INPUT_KEY_M))
        ToggleMuted(application);
    if (InputConsumeKeyPress(application->input, INPUT_KEY_R))
    {
        application->onlinePaused = false;
        application->message[0] = L'\0';
        RequestOnlineQueue(application);
    }
}

static void OnFrame(void* userData)
{
    Application* application = userData;
    if (WindowConsumeFocusLoss(application->window))
        InputResetState(application->input);
    if (WindowConsumeResize(application->window))
    {
        WindowGetClientSize(application->window,
            &application->width, &application->height);
        UiRendererResize(application->renderer,
            application->width, application->height);
    }

    PollRadioSource(application);
    PollAudio(application);
    AudioPlayerSnapshot snapshot = { 0 };
    AudioPlayerGetSnapshot(application->audio, &snapshot);
    HandleKeyboard(application, &snapshot);

    double now = PlatformTimeSeconds();
    float delta = (float)(now - application->previousTime);
    application->previousTime = now;
    int32_t mouseX;
    int32_t mouseY;
    WindowGetCursorClientPosition(application->window, &mouseX, &mouseY);
    bool uiReady = UiBegin(&application->ui,
        application->width, application->height,
        (float)mouseX, (float)mouseY,
        InputIsMouseButtonDown(application->input, INPUT_MOUSE_BUTTON_LEFT),
        InputWasMouseButtonPressed(application->input, INPUT_MOUSE_BUTTON_LEFT),
        WindowConsumeMouseWheelSteps(application->window), delta);
    if (uiReady && application->ui.fontDirty)
    {
        if (UiRendererSetFontAtlas(application->renderer,
            application->ui.font.atlas,
            application->ui.font.atlasWidth,
            application->ui.font.atlasHeight))
            application->ui.fontDirty = false;
    }
    if (uiReady) DrawPlayer(application, &snapshot);

    if (UiRendererBeginFrame(application->renderer,
        0.0035f, 0.0045f, 0.0048f))
    {
        if (uiReady)
            UiRendererQueue(application->renderer,
                application->ui.quads, application->ui.quadCount);
        if (!UiRendererEndFrame(application->renderer))
            WindowRequestClose(application->window);
    }
    InputEndFrame(application->input);
}

static bool InitializeApplication(Application* application, Window* window,
    Input* input, UiRenderer* renderer)
{
    application->window = window;
    application->input = input;
    application->renderer = renderer;
    WindowGetClientSize(window, &application->width, &application->height);
    application->previousTime = PlatformTimeSeconds();
    PlayerStateLoad(&application->saved);

    AudioPlayerConfiguration audioConfiguration = {
        .initialVolume = application->saved.volume,
        .initiallyMuted = application->saved.muted,
        .looping = false,
    };
    if (AudioPlayerCreate(&audioConfiguration, &application->audio)
        != AUDIO_RESULT_OK) return false;
    application->source = RadioSourceCreate();

    if (application->saved.historyCursor >= 0)
    {
        Track* previous = &application->saved.history[
            application->saved.historyCursor];
        if (TrackFileExists(previous))
            PlayTrack(application, previous, false, false);
    }
    RequestOnlineQueue(application);
    return true;
}

static void ReleaseApplication(Application* application)
{
    PlayerStateSave(&application->saved);
    if (application->source != NULL) RadioSourceDestroy(application->source);
    if (application->audio != NULL) AudioPlayerDestroy(application->audio);
    UiRelease(&application->ui);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previousInstance,
    PWSTR commandLine, int showCommand)
{
    (void)instance;
    (void)previousInstance;
    (void)showCommand;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    UINT systemDpi = GetDpiForSystem();
    WindowConfiguration configuration = {
        .title = L"laiue radio — video game music",
        .width = MulDiv(1180, (int)systemDpi, 96),
        .height = MulDiv(760, (int)systemDpi, 96),
    };
    Window* window = WindowCreate(&configuration);
    if (window == NULL) return 1;
    Input* input = InputCreate(WindowGetNativeHandle(window));
    if (input == NULL) { WindowDestroy(window); return 2; }
    int32_t width;
    int32_t height;
    WindowGetClientSize(window, &width, &height);
    UiRenderer* renderer = UiRendererCreate(
        WindowGetNativeHandle(window), width, height);
    if (renderer == NULL)
    {
        InputDestroy(input);
        WindowDestroy(window);
        return 3;
    }

    Application* application = HeapAlloc(GetProcessHeap(),
        HEAP_ZERO_MEMORY, sizeof(*application));
    if (application == NULL
        || !InitializeApplication(application, window, input, renderer))
    {
        MessageBoxW(NULL, L"Не удалось запустить аудиосистему Media Foundation.",
            L"laiue radio", MB_ICONERROR | MB_OK);
        if (application != NULL)
        {
            ReleaseApplication(application);
            HeapFree(GetProcessHeap(), 0, application);
        }
        UiRendererDestroy(renderer);
        InputDestroy(input);
        WindowDestroy(window);
        return 4;
    }

    WindowSetRawInputCallback(window, HandleRawInput, input);
    if (commandLine != NULL && commandLine[0] != L'\0')
    {
        int argumentCount = 0;
        wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(),
            &argumentCount);
        if (arguments != NULL && argumentCount > 1)
        {
            Track track;
            TrackFromLocalFile(&track, arguments[1]);
            PlayTrack(application, &track, true, true);
        }
        if (arguments != NULL) LocalFree(arguments);
    }
    WindowRunLoop(window, OnFrame, application);

    ReleaseApplication(application);
    HeapFree(GetProcessHeap(), 0, application);
    UiRendererDestroy(renderer);
    InputDestroy(input);
    WindowDestroy(window);
    return 0;
}
