import test from "node:test";
import assert from "node:assert/strict";
import {
  isAllowedAudioSource,
  parseTrackPage,
  toClientTrack,
} from "../server/khinsider.js";

const PAGE_URL =
  "https://downloads.khinsider.com/game-soundtracks/album/example-game/01.%2520Quiet%2520Place.mp3";

test("parseTrackPage extracts track metadata and the direct MP3", () => {
  const html = `
    <html>
      <head><meta property="og:image" content="/covers/example.webp"></head>
      <body>
        <h2>Example Game Soundtrack</h2>
        <p>Go back to the album - <a href="/game-soundtracks/album/example-game">Example Game</a></p>
        <p>Album name: Example Game OST</p>
        <p>Song name: Quiet Place</p>
        <a href="https://eta.vgmtreasurechest.com/soundtracks/example/01%20Quiet%20Place.mp3">
          Click here to download as MP3
        </a>
      </body>
    </html>
  `;

  const track = parseTrackPage(html, PAGE_URL);

  assert.equal(track.title, "Quiet Place");
  assert.equal(track.album, "Example Game OST");
  assert.equal(track.artworkUrl, "https://downloads.khinsider.com/covers/example.webp");
  assert.equal(
    track.albumPageUrl,
    "https://downloads.khinsider.com/game-soundtracks/album/example-game",
  );
  assert.match(track.id, /^[a-f0-9]{20}$/);
});

test("audio source validation blocks arbitrary hosts and insecure URLs", () => {
  assert.equal(
    isAllowedAudioSource("https://eta.vgmtreasurechest.com/music/song.mp3"),
    true,
  );
  assert.equal(isAllowedAudioSource("https://evil.example/song.mp3"), false);
  assert.equal(isAllowedAudioSource("http://eta.vgmtreasurechest.com/song.mp3"), false);
  assert.equal(isAllowedAudioSource("https://eta.vgmtreasurechest.com/archive.zip"), false);
});

test("client track hides the direct source behind the local stream endpoint", () => {
  const track = parseTrackPage(
    `<body>
       Album name: Album Song name: Song
       <a href="https://eta.vgmtreasurechest.com/music/song.mp3">Click here</a>
     </body>`,
    PAGE_URL,
  );
  const clientTrack = toClientTrack(track);

  assert.equal("audioUrl" in clientTrack, false);
  assert.match(clientTrack.streamUrl, /^\/api\/stream\?/);
  assert.match(clientTrack.streamUrl, /src=/);
});
