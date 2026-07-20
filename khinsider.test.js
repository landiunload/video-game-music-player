import { createHash } from "node:crypto";
import * as cheerio from "cheerio";

export const SOURCE_ORIGIN = "https://downloads.khinsider.com";
export const RANDOM_SONG_URL = `${SOURCE_ORIGIN}/random-song`;

const REQUEST_HEADERS = {
  accept: "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
  "accept-language": "en-US,en;q=0.8",
  "user-agent":
    "Mozilla/5.0 (compatible; laiue-radio/1.0; +https://github.com/landiunload/video-game-music-player)",
};

const AUDIO_EXTENSIONS = /\.(?:mp3|ogg|m4a)(?:$|[?#])/i;
const ALLOWED_AUDIO_ROOTS = ["vgmtreasurechest.com", "khinsider.com"];

function normalizeText(value) {
  return value.replace(/\u00a0/g, " ").replace(/\s+/g, " ").trim();
}

function absoluteUrl(value, baseUrl) {
  if (!value) return null;

  try {
    return new URL(value, baseUrl).href;
  } catch {
    return null;
  }
}

function labelValue(text, label, nextLabel) {
  const start = text.toLowerCase().indexOf(label.toLowerCase());
  if (start === -1) return "";

  const valueStart = start + label.length;
  const remainder = text.slice(valueStart);
  const end = remainder.toLowerCase().indexOf(nextLabel.toLowerCase());
  return normalizeText(end === -1 ? remainder : remainder.slice(0, end));
}

function titleFromPath(pageUrl) {
  try {
    const filename = new URL(pageUrl).pathname.split("/").at(-1) ?? "Unknown track";
    let decoded = filename;

    for (let attempt = 0; attempt < 2; attempt += 1) {
      try {
        decoded = decodeURIComponent(decoded);
      } catch {
        break;
      }
    }

    return decoded
      .replace(/\.(?:mp3|ogg|m4a)$/i, "")
      .replace(/^\d+[.\s_-]+/, "")
      .trim();
  } catch {
    return "Unknown track";
  }
}

function stableTrackId(pageUrl) {
  return createHash("sha256").update(pageUrl).digest("hex").slice(0, 20);
}

export function isAllowedAudioSource(value) {
  try {
    const url = new URL(value);
    if (url.protocol !== "https:" || !AUDIO_EXTENSIONS.test(url.href)) return false;

    return ALLOWED_AUDIO_ROOTS.some(
      (root) => url.hostname === root || url.hostname.endsWith(`.${root}`),
    );
  } catch {
    return false;
  }
}

export function parseTrackPage(html, pageUrl) {
  const $ = cheerio.load(html);
  const bodyText = normalizeText($("body").text());
  const album =
    labelValue(bodyText, "Album name:", "Song name:") ||
    normalizeText($("h2").first().text()) ||
    "Unknown album";
  const title =
    labelValue(bodyText, "Song name:", "Click here")
      .replace(/^get_app\s*/i, "")
      .trim() || titleFromPath(pageUrl);

  const links = $("a[href]")
    .toArray()
    .map((element) => absoluteUrl($(element).attr("href"), pageUrl))
    .filter(Boolean);

  const audioUrl =
    links.find((url) => {
      try {
        return isAllowedAudioSource(url) && new URL(url).hostname !== "downloads.khinsider.com";
      } catch {
        return false;
      }
    }) ?? links.find(isAllowedAudioSource);
  if (!audioUrl) {
    throw new Error("The source page did not contain a playable audio link.");
  }

  const albumPageUrl =
    links.find((url) => {
      try {
        const parsed = new URL(url);
        return (
          parsed.origin === SOURCE_ORIGIN &&
          parsed.pathname.includes("/game-soundtracks/album/") &&
          !AUDIO_EXTENSIONS.test(parsed.href)
        );
      } catch {
        return false;
      }
    }) ?? null;

  const artworkUrl =
    absoluteUrl($("meta[property='og:image']").attr("content"), pageUrl) ||
    absoluteUrl($("meta[name='twitter:image']").attr("content"), pageUrl) ||
    null;

  return {
    id: stableTrackId(pageUrl),
    title,
    album,
    artworkUrl,
    audioUrl,
    pageUrl,
    albumPageUrl,
  };
}

async function fetchWithTimeout(url, options = {}, timeoutMs = 14_000) {
  const timeoutController = new AbortController();
  const timeout = setTimeout(() => timeoutController.abort(), timeoutMs);

  try {
    const response = await fetch(url, {
      ...options,
      signal: timeoutController.signal,
    });

    if (!response.ok) {
      throw new Error(`The source returned HTTP ${response.status}.`);
    }

    return response;
  } finally {
    clearTimeout(timeout);
  }
}

export async function fetchRandomTrack() {
  let lastError;

  for (let attempt = 0; attempt < 3; attempt += 1) {
    try {
      const response = await fetchWithTimeout(RANDOM_SONG_URL, {
        headers: REQUEST_HEADERS,
        redirect: "follow",
      });
      const finalUrl = new URL(response.url);

      if (
        finalUrl.origin !== SOURCE_ORIGIN ||
        !finalUrl.pathname.includes("/game-soundtracks/album/")
      ) {
        throw new Error("The random-song redirect pointed outside the expected source.");
      }

      return parseTrackPage(await response.text(), finalUrl.href);
    } catch (error) {
      lastError = error;
    }
  }

  throw lastError ?? new Error("Could not load a random track.");
}

export function toClientTrack(track) {
  const params = new URLSearchParams({
    src: track.audioUrl,
    ref: track.pageUrl,
  });

  return {
    id: track.id,
    title: track.title,
    album: track.album,
    artworkUrl: track.artworkUrl,
    pageUrl: track.pageUrl,
    albumPageUrl: track.albumPageUrl,
    streamUrl: `/api/stream?${params}`,
  };
}

export function sourceRequestHeaders(referer) {
  return {
    ...REQUEST_HEADERS,
    accept: "audio/mpeg,audio/ogg,audio/*;q=0.9,*/*;q=0.5",
    referer:
      typeof referer === "string" && referer.startsWith(`${SOURCE_ORIGIN}/`)
        ? referer
        : `${SOURCE_ORIGIN}/`,
  };
}
