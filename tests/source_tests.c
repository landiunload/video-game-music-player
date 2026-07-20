#include "source/ocremix.h"

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

static void TestOcremixFixture(void)
{
    static const char html[] =
        "<html><head>"
        "<meta property=\"og:title\" content=\"Lufia II: Rise of the Sinistrals &quot;Idura's Desperation&quot; OC ReMix\">"
        "<meta property=\"og:image\" content=\"https://ocremix.org/files/images/games/snes/lufia.png\">"
        "</head><body>"
        "<a href=\"https://iterations.org/files/music/remixes/Lufia_2_Idura%27s_Desperation_OC_ReMix.mp3\">Download</a>"
        "</body></html>";
    Track track;
    wchar_t error[384];
    CHECK(OcremixParseTrackPage(html, strlen(html),
        L"https://ocremix.org/remix/OCR05046", &track, error, 384));
    CHECK(wcscmp(track.title, L"Idura's Desperation") == 0);
    CHECK(wcscmp(track.album, L"Lufia II: Rise of the Sinistrals") == 0);
    CHECK(OcremixIsAllowedAudioUrl(track.audioUrl));
    CHECK(wcscmp(track.artworkUrl,
        L"https://ocremix.org/files/images/games/snes/lufia.png") == 0);
    CHECK(track.id != 0);
}

static void TestRejectsUnsafeSources(void)
{
    CHECK(!OcremixIsAllowedAudioUrl(
        L"http://iterations.org/files/music/remixes/song.mp3"));
    CHECK(!OcremixIsAllowedAudioUrl(
        L"https://iterations.org.example.com/files/song.mp3"));
    CHECK(!OcremixIsAllowedAudioUrl(
        L"https://iterations.org/files/music/remixes/song.exe"));
    static const char incomplete[] = "<html><body>No audio</body></html>";
    Track track;
    wchar_t error[384];
    CHECK(!OcremixParseTrackPage(incomplete, strlen(incomplete),
        L"https://ocremix.org/remix/OCR00000", &track, error, 384));
    CHECK(error[0] != L'\0');
}

int main(void)
{
    TestOcremixFixture();
    TestRejectsUnsafeSources();
    if (failures == 0) printf("online source parsers: all tests passed\n");
    return failures == 0 ? 0 : 1;
}
