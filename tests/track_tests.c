#include "track.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

// Юнит-тесты модели трека. TrackHashText и TrackFromLocalFile — чистая логика
// (разбор пути, отсечение расширения, стабильный хеш), но её не проверял ни один
// тест: ошибка в выделении имени файла или в отсечении расширения прошла бы молча.

static int failures;

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (0)

static void TestHashTextSeedDeterminismAndDistinctness(void)
{
    // FNV-1a: NULL и пустая строка дают затравку без единого шага перемешивания.
    const uint64_t seed = UINT64_C(14695981039346656037);
    CHECK(TrackHashText(NULL) == seed);
    CHECK(TrackHashText(L"") == seed);

    // Одинаковый вход — одинаковый хеш; хеш непустого текста уходит от затравки.
    CHECK(TrackHashText(L"Chrono Trigger") == TrackHashText(L"Chrono Trigger"));
    CHECK(TrackHashText(L"Chrono Trigger") != seed);

    // Различие в одном символе меняет результат (в том числе в старшем байте).
    CHECK(TrackHashText(L"Chrono Trigger") != TrackHashText(L"Chrono Triggen"));
    CHECK(TrackHashText(L"A") != TrackHashText(L"\x0100"));
}

static void TestLocalFileExtractsTitleAndMarksLocal(void)
{
    Track track;
    TrackFromLocalFile(&track, L"C:\\Music\\Chrono Trigger.mp3");

    // Имя файла без расширения становится заголовком; альбом — фиксированный.
    CHECK(wcscmp(track.title, L"Chrono Trigger") == 0);
    CHECK(wcscmp(track.album, L"Локальная музыка") == 0);
    CHECK(track.local);

    // id — хеш нормализованного пути, который лёг в audioPath (проверяем инвариант,
    // не завязываясь на конкретное значение хеша).
    CHECK(track.id != 0);
    CHECK(track.id == TrackHashText(track.audioPath));
}

static void TestLocalFileStripsOnlyLastExtension(void)
{
    Track track;
    TrackFromLocalFile(&track, L"C:\\Music\\a.b.c.flac");

    // Отсекается только последнее расширение; точки внутри имени сохраняются.
    CHECK(wcscmp(track.title, L"a.b.c") == 0);
}

static void TestLocalFileWithoutExtensionKeepsWholeName(void)
{
    Track track;
    TrackFromLocalFile(&track, L"C:\\Music\\ready_player_one");

    CHECK(wcscmp(track.title, L"ready_player_one") == 0);
}

static void TestLocalFileNullArgumentsDoNotCrash(void)
{
    Track track;

    // Охранные условия: ни один из вызовов с NULL не должен падать.
    TrackFromLocalFile(NULL, L"C:\\Music\\x.mp3");
    TrackFromLocalFile(&track, NULL);
    CHECK(1);
}

int main(void)
{
    TestHashTextSeedDeterminismAndDistinctness();
    TestLocalFileExtractsTitleAndMarksLocal();
    TestLocalFileStripsOnlyLastExtension();
    TestLocalFileWithoutExtensionKeepsWholeName();
    TestLocalFileNullArgumentsDoNotCrash();

    if (failures == 0) printf("track: all tests passed\n");
    return failures == 0 ? 0 : 1;
}
