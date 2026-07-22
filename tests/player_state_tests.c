#include "player_state.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (0)

// Трек, определяемый только идентификатором: историю воспроизведения
// PlayerStateRecord ведёт по id, остальные поля для неё не важны.
static Track TrackWithId(uint64_t id)
{
    Track track;
    memset(&track, 0, sizeof(track));
    track.id = id;
    return track;
}

static void TestDefaults(void)
{
    PlayerState state;
    PlayerStateDefaults(&state);
    CHECK(state.historyCount == 0);
    CHECK(state.historyCursor == -1);
    CHECK(state.muted == false);
    CHECK(state.volume > 0.71f && state.volume < 0.73f);
}

static void TestRecordAppendsDistinctTracks(void)
{
    PlayerState state;
    PlayerStateDefaults(&state);

    Track first = TrackWithId(1);
    PlayerStateRecord(&state, &first);
    CHECK(state.historyCount == 1);
    CHECK(state.historyCursor == 0);
    CHECK(state.history[0].id == UINT64_C(1));

    Track second = TrackWithId(2);
    PlayerStateRecord(&state, &second);
    CHECK(state.historyCount == 2);
    CHECK(state.historyCursor == 1);
    CHECK(state.history[1].id == UINT64_C(2));
}

// Повторная запись трека, уже находящегося под курсором, дубликата не создаёт.
static void TestRecordSameTrackAtCursorIsNoOp(void)
{
    PlayerState state;
    PlayerStateDefaults(&state);

    Track track = TrackWithId(7);
    PlayerStateRecord(&state, &track);
    PlayerStateRecord(&state, &track);
    CHECK(state.historyCount == 1);
    CHECK(state.historyCursor == 0);
}

// Тот же id, но не под курсором, — это возврат к прослушанному треку,
// его записываем как новый шаг истории.
static void TestRecordSameIdNotAtCursorAppends(void)
{
    PlayerState state;
    PlayerStateDefaults(&state);

    Track a = TrackWithId(1);
    Track b = TrackWithId(2);
    PlayerStateRecord(&state, &a);
    PlayerStateRecord(&state, &b);
    PlayerStateRecord(&state, &a);
    CHECK(state.historyCount == 3);
    CHECK(state.historyCursor == 2);
    CHECK(state.history[2].id == UINT64_C(1));
}

// Запись нового трека после шагов «назад» отбрасывает «прямую» историю —
// как ветвление в истории браузера.
static void TestRecordAfterSteppingBackTruncatesForwardHistory(void)
{
    PlayerState state;
    PlayerStateDefaults(&state);

    Track t1 = TrackWithId(1);
    Track t2 = TrackWithId(2);
    Track t3 = TrackWithId(3);
    PlayerStateRecord(&state, &t1);
    PlayerStateRecord(&state, &t2);
    PlayerStateRecord(&state, &t3);

    // Пользователь дважды нажал «назад» — курсор указывает на первый трек.
    state.historyCursor = 0;

    Track t4 = TrackWithId(4);
    PlayerStateRecord(&state, &t4);

    CHECK(state.historyCount == 2);
    CHECK(state.historyCursor == 1);
    CHECK(state.history[0].id == UINT64_C(1));
    CHECK(state.history[1].id == UINT64_C(4));
}

// При переполнении истории вытесняется самый старый трек, курсор остаётся в конце.
static void TestRecordEvictsOldestAtCapacity(void)
{
    PlayerState state;
    PlayerStateDefaults(&state);

    for (uint64_t id = 1; id <= (uint64_t)PLAYER_HISTORY_CAPACITY; ++id)
    {
        Track track = TrackWithId(id);
        PlayerStateRecord(&state, &track);
    }
    CHECK(state.historyCount == PLAYER_HISTORY_CAPACITY);
    CHECK(state.historyCursor == PLAYER_HISTORY_CAPACITY - 1);
    CHECK(state.history[0].id == UINT64_C(1));
    CHECK(state.history[PLAYER_HISTORY_CAPACITY - 1].id == (uint64_t)PLAYER_HISTORY_CAPACITY);

    Track overflow = TrackWithId((uint64_t)PLAYER_HISTORY_CAPACITY + 1);
    PlayerStateRecord(&state, &overflow);

    CHECK(state.historyCount == PLAYER_HISTORY_CAPACITY);
    CHECK(state.historyCursor == PLAYER_HISTORY_CAPACITY - 1);
    // Самый старый (id=1) вытеснен — теперь первым идёт id=2.
    CHECK(state.history[0].id == UINT64_C(2));
    CHECK(state.history[PLAYER_HISTORY_CAPACITY - 1].id == (uint64_t)PLAYER_HISTORY_CAPACITY + 1);
}

// NULL-аргументы не должны приводить к падению или изменению состояния.
static void TestRecordNullArgumentsAreSafe(void)
{
    Track track = TrackWithId(1);
    PlayerStateRecord(NULL, &track);

    PlayerState state;
    PlayerStateDefaults(&state);
    PlayerStateRecord(&state, NULL);
    CHECK(state.historyCount == 0);
    CHECK(state.historyCursor == -1);
}

int main(void)
{
    TestDefaults();
    TestRecordAppendsDistinctTracks();
    TestRecordSameTrackAtCursorIsNoOp();
    TestRecordSameIdNotAtCursorAppends();
    TestRecordAfterSteppingBackTruncatesForwardHistory();
    TestRecordEvictsOldestAtCapacity();
    TestRecordNullArgumentsAreSafe();
    if (failures == 0) printf("player state history: all tests passed\n");
    return failures == 0 ? 0 : 1;
}
