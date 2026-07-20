#include "source/khinsider.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int failures;

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (0)

static void TestFixture(void)
{
    static const char html[] =
        "<html><head><meta property='og:image' content='/covers/example.webp'>"
        "</head><body><h2>Example Game Soundtrack</h2>"
        "<p>Go back to the album - <a href='/game-soundtracks/album/example-game'>Example</a></p>"
        "<p>Album name: Example Game &amp; Friends OST</p>"
        "<p>Song name: Quiet Place</p>"
        "<a href='https://eta.vgmtreasurechest.com/soundtracks/example/01%20Quiet%20Place.mp3'>"
        "Click here to download as MP3</a></body></html>";
    Track track;
    wchar_t error[384];
    bool parsed = KhinsiderParseTrackPage(html, strlen(html),
        L"https://downloads.khinsider.com/game-soundtracks/album/example/quiet-place",
        &track, error, 384);
    CHECK(parsed);
    CHECK(wcscmp(track.title, L"Quiet Place") == 0);
    CHECK(wcscmp(track.album, L"Example Game & Friends OST") == 0);
    CHECK(wcscmp(track.audioUrl,
        L"https://eta.vgmtreasurechest.com/soundtracks/example/01%20Quiet%20Place.mp3") == 0);
    CHECK(wcsstr(track.artworkUrl, L"/covers/example.webp") != NULL);
    CHECK(track.id != 0);
}

static void TestAllowedAudioSources(void)
{
    CHECK(KhinsiderIsAllowedAudioUrl(
        L"https://eta.vgmtreasurechest.com/music/song.ogg"));
    CHECK(KhinsiderIsAllowedAudioUrl(
        L"https://downloads.khinsider.com/music/song.m4a"));
    CHECK(!KhinsiderIsAllowedAudioUrl(
        L"https://vgmtreasurechest.com.example.org/music/song.mp3"));
    CHECK(!KhinsiderIsAllowedAudioUrl(
        L"https://example.org/music/song.mp3"));
    CHECK(!KhinsiderIsAllowedAudioUrl(
        L"https://eta.vgmtreasurechest.com/music/song.exe"));
}

static void TestRejectsIncompletePage(void)
{
    static const char html[] = "<html><body>Song name: Missing audio</body></html>";
    Track track;
    wchar_t error[384];
    CHECK(!KhinsiderParseTrackPage(html, strlen(html),
        L"https://downloads.khinsider.com/game-soundtracks/album/missing",
        &track, error, 384));
    CHECK(error[0] != L'\0');
}

int main(void)
{
    TestFixture();
    TestAllowedAudioSources();
    TestRejectsIncompletePage();
    if (failures == 0) printf("khinsider parser: all tests passed\n");
    return failures == 0 ? 0 : 1;
}

