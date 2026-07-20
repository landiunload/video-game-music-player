const STORAGE_KEY = "laiue-radio:v1";
const HISTORY_LIMIT = 100;
const PRELOAD_COUNT = 2;

const elements = {
  shell: document.querySelector(".app-shell"),
  ambient: document.querySelector(".ambient"),
  randomSource: document.querySelector("#randomSource"),
  playlistList: document.querySelector("#playlistList"),
  emptyPlaylists: document.querySelector("#emptyPlaylists"),
  newPlaylist: document.querySelector("#newPlaylist"),
  modePill: document.querySelector("#modePill"),
  preloadLabel: document.querySelector("#preloadLabel"),
  coverWrap: document.querySelector("#coverWrap"),
  coverImage: document.querySelector("#coverImage"),
  trackTitle: document.querySelector("#trackTitle"),
  trackAlbum: document.querySelector("#trackAlbum"),
  progress: document.querySelector("#progress"),
  currentTime: document.querySelector("#currentTime"),
  duration: document.querySelector("#duration"),
  previous: document.querySelector("#previous"),
  playPause: document.querySelector("#playPause"),
  next: document.querySelector("#next"),
  addCurrent: document.querySelector("#addCurrent"),
  mute: document.querySelector("#mute"),
  volume: document.querySelector("#volume"),
  sourceLink: document.querySelector("#sourceLink"),
  loadingLayer: document.querySelector("#loadingLayer"),
  queueTabs: document.querySelector(".queue-tabs"),
  queueTitle: document.querySelector("#queueTitle"),
  queueCount: document.querySelector("#queueCount"),
  trackList: document.querySelector("#trackList"),
  queueEmpty: document.querySelector("#queueEmpty"),
  createDialog: document.querySelector("#createPlaylistDialog"),
  createForm: document.querySelector("#createPlaylistForm"),
  playlistName: document.querySelector("#playlistName"),
  addDialog: document.querySelector("#addToPlaylistDialog"),
  dialogPlaylists: document.querySelector("#dialogPlaylists"),
  createFromAdd: document.querySelector("#createFromAdd"),
  toast: document.querySelector("#toast"),
};

const audio = new Audio();
audio.preload = "auto";

let state = loadState();
let currentTrack = state.history[state.cursor] ?? null;
let randomQueue = [];
let activeQueueTab = "upcoming";
let randomRequest = null;
let preloaders = [];
let toastTimer = null;
let createAndAddCurrent = false;

function defaultState() {
  return {
    playlists: [],
    history: [],
    cursor: -1,
    source: { type: "random", playlistId: null },
    volume: 0.72,
    muted: false,
  };
}

function cleanTrack(value) {
  if (!value || typeof value !== "object") return null;
  if (
    typeof value.id !== "string" ||
    typeof value.title !== "string" ||
    typeof value.streamUrl !== "string" ||
    !value.streamUrl.startsWith("/api/stream?")
  ) {
    return null;
  }

  return {
    id: value.id.slice(0, 80),
    title: value.title.slice(0, 300),
    album: typeof value.album === "string" ? value.album.slice(0, 500) : "Unknown album",
    artworkUrl: typeof value.artworkUrl === "string" ? value.artworkUrl : null,
    pageUrl: typeof value.pageUrl === "string" ? value.pageUrl : null,
    albumPageUrl: typeof value.albumPageUrl === "string" ? value.albumPageUrl : null,
    streamUrl: value.streamUrl,
  };
}

function loadState() {
  const fallback = defaultState();

  try {
    const saved = JSON.parse(localStorage.getItem(STORAGE_KEY) ?? "null");
    if (!saved || typeof saved !== "object") return fallback;

    const playlists = Array.isArray(saved.playlists)
      ? saved.playlists
          .filter((playlist) => playlist && typeof playlist.name === "string")
          .map((playlist) => ({
            id: typeof playlist.id === "string" ? playlist.id : crypto.randomUUID(),
            name: playlist.name.trim().slice(0, 48) || "Без названия",
            tracks: Array.isArray(playlist.tracks)
              ? playlist.tracks.map(cleanTrack).filter(Boolean)
              : [],
          }))
      : [];
    const history = Array.isArray(saved.history)
      ? saved.history.map(cleanTrack).filter(Boolean).slice(-HISTORY_LIMIT)
      : [];
    const cursor = Math.min(
      Math.max(Number.isInteger(saved.cursor) ? saved.cursor : history.length - 1, -1),
      history.length - 1,
    );
    const sourcePlaylist = playlists.some(
      (playlist) => playlist.id === saved.source?.playlistId,
    );

    return {
      playlists,
      history,
      cursor,
      source:
        saved.source?.type === "playlist" && sourcePlaylist
          ? { type: "playlist", playlistId: saved.source.playlistId }
          : { type: "random", playlistId: null },
      volume:
        typeof saved.volume === "number"
          ? Math.min(Math.max(saved.volume, 0), 1)
          : fallback.volume,
      muted: Boolean(saved.muted),
    };
  } catch {
    return fallback;
  }
}

function saveState() {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(state));
}

function escapeHtml(value) {
  return String(value).replace(
    /[&<>'"]/g,
    (character) =>
      ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", "'": "&#39;", '"': "&quot;" })[
        character
      ],
  );
}

function isSafeArtwork(value) {
  if (!value) return false;
  try {
    return new URL(value).protocol === "https:";
  } catch {
    return false;
  }
}

function hueFor(value) {
  let hash = 0;
  for (const character of String(value)) {
    hash = (hash * 31 + character.codePointAt(0)) >>> 0;
  }
  return hash % 360;
}

function pluralTracks(count) {
  const lastTwo = count % 100;
  const last = count % 10;
  if (lastTwo >= 11 && lastTwo <= 14) return `${count} треков`;
  if (last === 1) return `${count} трек`;
  if (last >= 2 && last <= 4) return `${count} трека`;
  return `${count} треков`;
}

function formatTime(value) {
  if (!Number.isFinite(value) || value < 0) return "0:00";
  const totalSeconds = Math.floor(value);
  const minutes = Math.floor(totalSeconds / 60);
  const seconds = String(totalSeconds % 60).padStart(2, "0");
  return `${minutes}:${seconds}`;
}

function showToast(message) {
  clearTimeout(toastTimer);
  elements.toast.textContent = message;
  elements.toast.classList.add("is-visible");
  toastTimer = setTimeout(() => elements.toast.classList.remove("is-visible"), 2600);
}

function setLoading(visible, message = "Находим случайный трек") {
  elements.loadingLayer.querySelector("p").textContent = message;
  elements.loadingLayer.classList.toggle("is-visible", visible);
}

function activePlaylist() {
  if (state.source.type !== "playlist") return null;
  return state.playlists.find((playlist) => playlist.id === state.source.playlistId) ?? null;
}

async function requestRandomTracks(count) {
  const response = await fetch(`/api/tracks/random?count=${Math.min(Math.max(count, 1), 4)}`, {
    headers: { accept: "application/json" },
  });
  const data = await response.json().catch(() => ({}));

  if (!response.ok) {
    throw new Error(data.error || "Источник музыки временно не отвечает.");
  }

  return Array.isArray(data.tracks) ? data.tracks.map(cleanTrack).filter(Boolean) : [];
}

async function ensureRandomQueue(minimum = 3) {
  if (randomQueue.length >= minimum) return;
  if (randomRequest) {
    await randomRequest;
    return;
  }

  randomRequest = (async () => {
    const missing = Math.min(Math.max(minimum - randomQueue.length, 1), 4);
    const tracks = await requestRandomTracks(missing);
    const knownIds = new Set([currentTrack?.id, ...randomQueue.map((track) => track.id)]);

    for (const track of tracks) {
      if (!knownIds.has(track.id)) {
        knownIds.add(track.id);
        randomQueue.push(track);
      }
    }
  })().finally(() => {
    randomRequest = null;
    renderQueue();
    preloadUpcoming();
  });

  await randomRequest;
}

async function takeRandomTrack() {
  await ensureRandomQueue(3);
  const track = randomQueue.shift();
  if (!track) throw new Error("Не удалось получить новый случайный трек.");
  void ensureRandomQueue(3).catch(() => {});
  return track;
}

function recordInHistory(track) {
  const currentHistoryTrack = state.history[state.cursor];
  if (currentHistoryTrack?.id === track.id) return;

  state.history = state.history.slice(0, state.cursor + 1);
  state.history.push(track);

  if (state.history.length > HISTORY_LIMIT) {
    state.history = state.history.slice(-HISTORY_LIMIT);
  }

  state.cursor = state.history.length - 1;
}

async function setTrack(track, { record = true, autoplay = true } = {}) {
  if (!track) return;

  if (record) recordInHistory(track);
  currentTrack = track;
  saveState();
  renderCurrentTrack();
  renderQueue();
  preloadUpcoming();

  audio.pause();
  audio.src = track.streamUrl;
  audio.load();

  if (autoplay) {
    try {
      await audio.play();
    } catch (error) {
      if (error?.name === "NotAllowedError") {
        showToast("Нажми Play, чтобы браузер разрешил звук.");
      } else if (error?.name !== "AbortError") {
        showToast("Этот трек не удалось запустить.");
      }
    }
  }
}

async function nextTrack({ autoplay = true, useForwardHistory = true } = {}) {
  try {
    if (useForwardHistory && state.cursor < state.history.length - 1) {
      state.cursor += 1;
      saveState();
      await setTrack(state.history[state.cursor], { record: false, autoplay });
      return;
    }

    const playlist = activePlaylist();
    let next;

    if (playlist) {
      if (playlist.tracks.length === 0) {
        showToast("В этом плейлисте пока нет треков.");
        return;
      }
      const currentIndex = playlist.tracks.findIndex((track) => track.id === currentTrack?.id);
      next = playlist.tracks[(currentIndex + 1 + playlist.tracks.length) % playlist.tracks.length];
    } else {
      next = await takeRandomTrack();
    }

    await setTrack(next, { record: true, autoplay });
  } catch (error) {
    showToast(error instanceof Error ? error.message : "Не удалось включить следующий трек.");
  }
}

async function previousTrack() {
  if (state.cursor <= 0) {
    audio.currentTime = 0;
    return;
  }

  state.cursor -= 1;
  saveState();
  await setTrack(state.history[state.cursor], { record: false, autoplay: true });
}

async function togglePlayback() {
  if (!currentTrack) {
    setLoading(true);
    try {
      await setTrack(await takeRandomTrack(), { record: true, autoplay: true });
    } catch (error) {
      showToast(error instanceof Error ? error.message : "Не удалось найти музыку.");
    } finally {
      setLoading(false);
    }
    return;
  }

  if (audio.paused) {
    try {
      await audio.play();
    } catch {
      showToast("Не удалось продолжить воспроизведение.");
    }
  } else {
    audio.pause();
  }
}

function upcomingPlaylistTracks() {
  const playlist = activePlaylist();
  if (!playlist?.tracks.length) return [];
  const currentIndex = playlist.tracks.findIndex((track) => track.id === currentTrack?.id);
  const start = currentIndex === -1 ? 0 : currentIndex + 1;

  return Array.from({ length: Math.min(PRELOAD_COUNT, playlist.tracks.length) }, (_, offset) =>
    playlist.tracks[(start + offset) % playlist.tracks.length],
  );
}

function preloadUpcoming() {
  for (const preloader of preloaders) {
    preloader.removeAttribute("src");
    preloader.load();
  }
  preloaders = [];

  const upcoming =
    state.source.type === "playlist"
      ? upcomingPlaylistTracks()
      : randomQueue.slice(0, PRELOAD_COUNT);

  for (const track of upcoming) {
    const preloader = new Audio();
    preloader.preload = "auto";
    preloader.muted = true;
    preloader.src = track.streamUrl;
    preloader.load();
    preloaders.push(preloader);
  }

  elements.preloadLabel.textContent =
    upcoming.length > 0 ? `${pluralTracks(upcoming.length)} впереди` : "готовим очередь";
}

function renderSidebar() {
  const activeId = state.source.type === "playlist" ? state.source.playlistId : null;
  elements.randomSource.classList.toggle("is-active", state.source.type === "random");
  elements.playlistList.innerHTML = state.playlists
    .map((playlist) => {
      const hue = hueFor(playlist.id);
      return `
        <div class="playlist-row">
          <button class="playlist-button ${playlist.id === activeId ? "is-active" : ""}" data-playlist-id="${escapeHtml(playlist.id)}" type="button">
            <span class="playlist-cover" style="--playlist-hue:${hue}">${escapeHtml(playlist.name[0] ?? "l")}</span>
            <span class="playlist-label">
              <strong>${escapeHtml(playlist.name)}</strong>
              <small>${pluralTracks(playlist.tracks.length)}</small>
            </span>
          </button>
          <button class="icon-button playlist-delete" data-delete-playlist="${escapeHtml(playlist.id)}" type="button" aria-label="Удалить плейлист ${escapeHtml(playlist.name)}">
            <svg><use href="#i-trash"></use></svg>
          </button>
        </div>`;
    })
    .join("");
  elements.emptyPlaylists.hidden = state.playlists.length > 0;
}

function renderCurrentTrack() {
  const playlist = activePlaylist();
  elements.modePill.textContent = playlist ? playlist.name : "случайное радио";

  if (!currentTrack) {
    elements.trackTitle.textContent = "Ищем музыку…";
    elements.trackAlbum.textContent = "Один момент";
    elements.sourceLink.classList.add("is-disabled");
    return;
  }

  const hue = hueFor(currentTrack.id);
  document.documentElement.style.setProperty("--cover-hue", hue);
  elements.trackTitle.textContent = currentTrack.title;
  elements.trackAlbum.textContent = currentTrack.album;
  elements.sourceLink.href = currentTrack.pageUrl ?? "#";
  elements.sourceLink.classList.toggle("is-disabled", !currentTrack.pageUrl);

  elements.coverImage.classList.remove("is-loaded");
  if (isSafeArtwork(currentTrack.artworkUrl)) {
    elements.coverImage.src = currentTrack.artworkUrl;
  } else {
    elements.coverImage.removeAttribute("src");
  }

  updateMediaSession();
}

function trackRow(track, options = {}) {
  const artwork = isSafeArtwork(track.artworkUrl)
    ? `<img src="${escapeHtml(track.artworkUrl)}" alt="" loading="lazy">`
    : "";
  const removeButton = options.removable
    ? `<button class="icon-button track-remove" data-remove-track="${escapeHtml(track.id)}" type="button" aria-label="Убрать ${escapeHtml(track.title)}"><svg><use href="#i-close"></use></svg></button>`
    : `<span class="track-index">${String(options.number ?? 1).padStart(2, "0")}</span>`;

  return `
    <div class="track-row ${track.id === currentTrack?.id ? "is-current" : ""}"
      role="button" tabindex="0" data-action="${escapeHtml(options.action ?? "")}" data-track-id="${escapeHtml(track.id)}" ${options.historyIndex !== undefined ? `data-history-index="${options.historyIndex}"` : ""}>
      <span class="track-mini-cover" style="--track-hue:${hueFor(track.id)}">l${artwork}</span>
      <span class="track-row-copy"><strong>${escapeHtml(track.title)}</strong><small>${escapeHtml(track.album)}</small></span>
      ${removeButton}
    </div>`;
}

function renderQueue() {
  document.querySelectorAll(".queue-tab").forEach((button) => {
    button.classList.toggle("is-active", button.dataset.tab === activeQueueTab);
  });

  let rows = [];
  let title = "Случайная очередь";

  if (activeQueueTab === "history") {
    title = "Прослушанное";
    rows = state.history
      .map((track, index) => ({ track, historyIndex: index }))
      .reverse();
    elements.trackList.innerHTML = rows
      .map(({ track, historyIndex }, index) =>
        trackRow(track, { action: "history", historyIndex, number: index + 1 }),
      )
      .join("");
  } else {
    const playlist = activePlaylist();
    if (playlist) {
      title = playlist.name;
      rows = playlist.tracks;
      elements.trackList.innerHTML = rows
        .map((track, index) =>
          trackRow(track, { action: "playlist", number: index + 1, removable: true }),
        )
        .join("");
    } else {
      rows = randomQueue;
      elements.trackList.innerHTML = rows
        .map((track, index) => trackRow(track, { action: "random", number: index + 1 }))
        .join("");
    }
  }

  elements.queueTitle.textContent = title;
  elements.queueCount.textContent = pluralTracks(rows.length);
  elements.queueEmpty.classList.toggle("is-visible", rows.length === 0);
}

function renderVolume() {
  audio.volume = state.volume;
  audio.muted = state.muted;
  elements.volume.value = String(state.volume);
  elements.volume.style.setProperty("--range-progress", `${state.volume * 100}%`);
  elements.mute.classList.toggle("is-muted", state.muted || state.volume === 0);
}

function renderAll() {
  renderSidebar();
  renderCurrentTrack();
  renderQueue();
  renderVolume();
}

function updateMediaSession() {
  if (!("mediaSession" in navigator) || !("MediaMetadata" in window) || !currentTrack) return;

  const artwork = isSafeArtwork(currentTrack.artworkUrl)
    ? [{ src: currentTrack.artworkUrl }]
    : [{ src: "/icon.svg", sizes: "512x512", type: "image/svg+xml" }];

  navigator.mediaSession.metadata = new MediaMetadata({
    title: currentTrack.title,
    artist: "laiue radio",
    album: currentTrack.album,
    artwork,
  });
}

function addTrackToPlaylist(playlistId) {
  const playlist = state.playlists.find((candidate) => candidate.id === playlistId);
  if (!playlist || !currentTrack) return;

  if (playlist.tracks.some((track) => track.id === currentTrack.id)) {
    showToast("Этот трек уже есть в плейлисте.");
    return;
  }

  playlist.tracks.push(currentTrack);
  saveState();
  renderSidebar();
  renderQueue();
  showToast(`Добавлено в «${playlist.name}».`);
}

function renderAddDialog() {
  elements.dialogPlaylists.innerHTML = state.playlists
    .map(
      (playlist) => `
        <button class="dialog-playlist-button" data-add-to="${escapeHtml(playlist.id)}" type="button">
          <strong>${escapeHtml(playlist.name)}</strong><small>${pluralTracks(playlist.tracks.length)}</small>
        </button>`,
    )
    .join("");
}

function openCreateDialog({ addCurrent = false } = {}) {
  createAndAddCurrent = addCurrent;
  elements.playlistName.value = "";
  elements.createDialog.showModal();
  requestAnimationFrame(() => elements.playlistName.focus());
}

async function activateRandom() {
  state.source = { type: "random", playlistId: null };
  saveState();
  renderAll();
  setLoading(true, "Ищем новый случайный трек");
  try {
    await setTrack(await takeRandomTrack(), { record: true, autoplay: true });
  } catch (error) {
    showToast(error instanceof Error ? error.message : "Не удалось найти музыку.");
  } finally {
    setLoading(false);
  }
}

async function activatePlaylist(playlistId) {
  const playlist = state.playlists.find((candidate) => candidate.id === playlistId);
  if (!playlist) return;

  state.source = { type: "playlist", playlistId };
  saveState();
  renderAll();

  if (playlist.tracks.length === 0) {
    showToast("Добавь сюда трек из случайного радио.");
    return;
  }

  await setTrack(playlist.tracks[0], { record: true, autoplay: true });
}

async function playQueueRow(row) {
  const action = row.dataset.action;
  const trackId = row.dataset.trackId;

  if (action === "history") {
    const historyIndex = Number.parseInt(row.dataset.historyIndex, 10);
    if (!Number.isInteger(historyIndex) || !state.history[historyIndex]) return;
    state.cursor = historyIndex;
    saveState();
    await setTrack(state.history[historyIndex], { record: false, autoplay: true });
    return;
  }

  if (action === "playlist") {
    const playlist = activePlaylist();
    const track = playlist?.tracks.find((candidate) => candidate.id === trackId);
    if (track) await setTrack(track, { record: true, autoplay: true });
    return;
  }

  if (action === "random") {
    const index = randomQueue.findIndex((candidate) => candidate.id === trackId);
    if (index === -1) return;
    const [track] = randomQueue.splice(index, 1);
    await setTrack(track, { record: true, autoplay: true });
    void ensureRandomQueue(3).catch(() => {});
  }
}

elements.coverImage.addEventListener("load", () => {
  elements.coverImage.classList.add("is-loaded");
});

elements.coverImage.addEventListener("error", () => {
  elements.coverImage.classList.remove("is-loaded");
});

audio.addEventListener("play", () => {
  elements.shell.classList.add("is-playing");
  elements.playPause.setAttribute("aria-label", "Пауза");
  if ("mediaSession" in navigator) navigator.mediaSession.playbackState = "playing";
});

audio.addEventListener("pause", () => {
  elements.shell.classList.remove("is-playing");
  elements.playPause.setAttribute("aria-label", "Воспроизвести");
  if ("mediaSession" in navigator) navigator.mediaSession.playbackState = "paused";
});

audio.addEventListener("loadedmetadata", () => {
  elements.duration.textContent = formatTime(audio.duration);
});

audio.addEventListener("timeupdate", () => {
  const ratio = Number.isFinite(audio.duration) && audio.duration > 0 ? audio.currentTime / audio.duration : 0;
  elements.progress.value = String(Math.round(ratio * 1000));
  elements.progress.style.setProperty("--range-progress", `${ratio * 100}%`);
  elements.currentTime.textContent = formatTime(audio.currentTime);
});

audio.addEventListener("ended", () => {
  void nextTrack({ autoplay: true, useForwardHistory: true });
});

audio.addEventListener("error", () => {
  if (audio.src) showToast("Источник не отдал этот трек. Можно нажать «Следующий».");
});

elements.progress.addEventListener("input", () => {
  if (!Number.isFinite(audio.duration)) return;
  audio.currentTime = (Number(elements.progress.value) / 1000) * audio.duration;
});

elements.volume.addEventListener("input", () => {
  state.volume = Number(elements.volume.value);
  if (state.volume > 0) state.muted = false;
  saveState();
  renderVolume();
});

elements.mute.addEventListener("click", () => {
  state.muted = !state.muted;
  saveState();
  renderVolume();
});

elements.playPause.addEventListener("click", () => void togglePlayback());
elements.previous.addEventListener("click", () => void previousTrack());
elements.next.addEventListener("click", () => void nextTrack());
elements.randomSource.addEventListener("click", () => void activateRandom());

elements.playlistList.addEventListener("click", (event) => {
  const deleteButton = event.target.closest("[data-delete-playlist]");
  if (deleteButton) {
    const playlist = state.playlists.find(
      (candidate) => candidate.id === deleteButton.dataset.deletePlaylist,
    );
    if (!playlist || !confirm(`Удалить плейлист «${playlist.name}»?`)) return;

    state.playlists = state.playlists.filter((candidate) => candidate.id !== playlist.id);
    if (state.source.playlistId === playlist.id) {
      state.source = { type: "random", playlistId: null };
    }
    saveState();
    renderAll();
    preloadUpcoming();
    showToast("Плейлист удалён.");
    return;
  }

  const button = event.target.closest("[data-playlist-id]");
  if (button) void activatePlaylist(button.dataset.playlistId);
});

elements.queueTabs.addEventListener("click", (event) => {
  const tab = event.target.closest("[data-tab]");
  if (!tab) return;
  activeQueueTab = tab.dataset.tab;
  renderQueue();
});

elements.trackList.addEventListener("click", (event) => {
  const removeButton = event.target.closest("[data-remove-track]");
  if (removeButton) {
    const playlist = activePlaylist();
    if (!playlist) return;
    playlist.tracks = playlist.tracks.filter(
      (track) => track.id !== removeButton.dataset.removeTrack,
    );
    saveState();
    renderAll();
    preloadUpcoming();
    showToast("Трек убран из плейлиста.");
    return;
  }

  const row = event.target.closest(".track-row");
  if (row) void playQueueRow(row);
});

elements.trackList.addEventListener("keydown", (event) => {
  if (event.key !== "Enter" && event.key !== " ") return;
  const row = event.target.closest(".track-row");
  if (!row || event.target.closest("button")) return;
  event.preventDefault();
  void playQueueRow(row);
});

elements.newPlaylist.addEventListener("click", () => openCreateDialog());

elements.addCurrent.addEventListener("click", () => {
  if (!currentTrack) {
    showToast("Сначала включи какой-нибудь трек.");
    return;
  }
  if (state.playlists.length === 0) {
    openCreateDialog({ addCurrent: true });
    return;
  }
  renderAddDialog();
  elements.addDialog.showModal();
});

elements.createFromAdd.addEventListener("click", () => {
  elements.addDialog.close();
  openCreateDialog({ addCurrent: true });
});

elements.dialogPlaylists.addEventListener("click", (event) => {
  const button = event.target.closest("[data-add-to]");
  if (!button) return;
  addTrackToPlaylist(button.dataset.addTo);
  elements.addDialog.close();
});

elements.createForm.addEventListener("submit", (event) => {
  event.preventDefault();
  const name = elements.playlistName.value.trim();
  if (!name) return;

  const playlist = {
    id: crypto.randomUUID(),
    name: name.slice(0, 48),
    tracks: createAndAddCurrent && currentTrack ? [currentTrack] : [],
  };
  state.playlists.push(playlist);
  saveState();
  renderAll();
  elements.createDialog.close();
  showToast(createAndAddCurrent ? `Трек сохранён в «${playlist.name}».` : "Плейлист создан.");
  createAndAddCurrent = false;
});

document.querySelectorAll("[data-close-dialog]").forEach((button) => {
  button.addEventListener("click", () => button.closest("dialog").close());
});

document.querySelectorAll("dialog").forEach((dialog) => {
  dialog.addEventListener("click", (event) => {
    if (event.target === dialog) dialog.close();
  });
});

document.addEventListener("keydown", (event) => {
  if (
    event.target.matches("input, button") ||
    document.querySelector("dialog[open]") ||
    event.ctrlKey ||
    event.metaKey ||
    event.altKey
  ) {
    return;
  }

  if (event.code === "Space") {
    event.preventDefault();
    void togglePlayback();
  } else if (event.code === "ArrowRight") {
    void nextTrack();
  } else if (event.code === "ArrowLeft") {
    void previousTrack();
  }
});

if ("mediaSession" in navigator) {
  navigator.mediaSession.setActionHandler("play", () => void togglePlayback());
  navigator.mediaSession.setActionHandler("pause", () => audio.pause());
  navigator.mediaSession.setActionHandler("previoustrack", () => void previousTrack());
  navigator.mediaSession.setActionHandler("nexttrack", () => void nextTrack());
  navigator.mediaSession.setActionHandler("seekto", ({ seekTime }) => {
    if (Number.isFinite(seekTime)) audio.currentTime = seekTime;
  });
}

async function bootstrap() {
  renderAll();

  if (currentTrack) {
    audio.src = currentTrack.streamUrl;
    audio.load();
  } else {
    setLoading(true);
  }

  try {
    await ensureRandomQueue(3);
    if (!currentTrack) {
      await setTrack(randomQueue.shift(), { record: true, autoplay: false });
      void ensureRandomQueue(3).catch(() => {});
    }
  } catch (error) {
    if (!currentTrack) {
      elements.trackTitle.textContent = "Музыка пока недоступна";
      elements.trackAlbum.textContent = "Попробуй ещё раз через минуту";
    }
    showToast(error instanceof Error ? error.message : "Не удалось загрузить очередь.");
  } finally {
    setLoading(false);
    preloadUpcoming();
  }
}

void bootstrap();
