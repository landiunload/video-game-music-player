:root {
  color-scheme: dark;
  --bg: #08090b;
  --shell: rgba(18, 20, 24, 0.93);
  --panel: #16181d;
  --panel-soft: #1b1e24;
  --line: rgba(255, 255, 255, 0.075);
  --text: #f5f3ed;
  --muted: #90949d;
  --quiet: #636872;
  --accent: #c9ff68;
  --accent-rgb: 201, 255, 104;
  --danger: #ff7d78;
  --cover-hue: 83;
  --radius: 26px;
  font-family: Inter, ui-sans-serif, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  font-synthesis: none;
}

* {
  box-sizing: border-box;
}

html {
  min-width: 320px;
  min-height: 100%;
  background: var(--bg);
}

body {
  min-height: 100vh;
  margin: 0;
  display: grid;
  place-items: center;
  overflow-x: hidden;
  color: var(--text);
  background:
    radial-gradient(circle at 18% 22%, rgba(106, 91, 255, 0.09), transparent 29rem),
    radial-gradient(circle at 85% 82%, rgba(201, 255, 104, 0.06), transparent 26rem),
    #08090b;
}

button,
input {
  font: inherit;
}

button,
a {
  -webkit-tap-highlight-color: transparent;
}

button {
  color: inherit;
}

button:not(:disabled),
a {
  cursor: pointer;
}

svg {
  width: 1.25rem;
  height: 1.25rem;
  fill: none;
  stroke: currentColor;
  stroke-width: 1.8;
  stroke-linecap: round;
  stroke-linejoin: round;
}

.svg-sprite {
  position: absolute;
  width: 0;
  height: 0;
  overflow: hidden;
}

.ambient {
  position: fixed;
  inset: -35%;
  pointer-events: none;
  background: radial-gradient(
    circle at 50% 45%,
    hsla(var(--cover-hue), 75%, 56%, 0.09),
    transparent 33%
  );
  filter: blur(30px);
  transition: background 800ms ease;
}

.app-shell {
  position: relative;
  width: min(1120px, calc(100vw - 40px));
  height: min(720px, calc(100vh - 40px));
  min-height: 620px;
  display: grid;
  grid-template-columns: 222px minmax(400px, 1fr) 290px;
  overflow: hidden;
  border: 1px solid rgba(255, 255, 255, 0.09);
  border-radius: var(--radius);
  background: var(--shell);
  box-shadow: 0 35px 100px rgba(0, 0, 0, 0.58), inset 0 1px rgba(255, 255, 255, 0.03);
  backdrop-filter: blur(24px);
}

.sidebar,
.queue-panel {
  min-width: 0;
  background: rgba(12, 13, 16, 0.66);
}

.sidebar {
  display: flex;
  flex-direction: column;
  padding: 27px 18px 19px;
  border-right: 1px solid var(--line);
}

.brand {
  display: inline-flex;
  align-items: center;
  gap: 11px;
  align-self: flex-start;
  margin: 0 8px 36px;
  color: var(--text);
  text-decoration: none;
  font-size: 17px;
  font-weight: 680;
  letter-spacing: -0.025em;
}

.brand small {
  display: block;
  margin-top: -2px;
  color: var(--muted);
  font-size: 9px;
  font-weight: 600;
  letter-spacing: 0.15em;
  text-transform: uppercase;
}

.brand-mark {
  width: 34px;
  height: 34px;
  display: grid;
  place-items: center;
  border-radius: 11px;
  color: #10120d;
  background: var(--accent);
  box-shadow: 0 0 25px rgba(var(--accent-rgb), 0.16);
  font-family: Georgia, serif;
  font-size: 22px;
  font-style: italic;
  font-weight: 700;
}

.source-nav {
  margin-bottom: 31px;
}

.source-button,
.playlist-button {
  width: 100%;
  min-width: 0;
  border: 0;
  text-align: left;
  background: transparent;
}

.source-button {
  position: relative;
  display: grid;
  grid-template-columns: 36px 1fr auto;
  align-items: center;
  gap: 10px;
  padding: 10px;
  border-radius: 13px;
  color: var(--muted);
  transition: color 160ms ease, background 160ms ease;
}

.source-button:hover,
.source-button.is-active {
  color: var(--text);
  background: rgba(255, 255, 255, 0.052);
}

.source-icon {
  width: 35px;
  height: 35px;
  display: grid;
  place-items: center;
  border-radius: 11px;
  background: rgba(var(--accent-rgb), 0.1);
  color: var(--accent);
}

.source-button strong,
.source-button small {
  display: block;
}

.source-button strong {
  overflow: hidden;
  font-size: 13px;
  font-weight: 620;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.source-button small {
  margin-top: 2px;
  color: var(--quiet);
  font-size: 10px;
}

.active-dot {
  width: 5px;
  height: 5px;
  border-radius: 50%;
  background: var(--accent);
  opacity: 0;
  box-shadow: 0 0 9px rgba(var(--accent-rgb), 0.8);
}

.is-active .active-dot {
  opacity: 1;
}

.playlists-section {
  min-height: 0;
  display: flex;
  flex: 1;
  flex-direction: column;
}

.section-heading {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 7px 9px 11px;
  color: var(--quiet);
  font-size: 10px;
  font-weight: 680;
  letter-spacing: 0.11em;
  text-transform: uppercase;
}

.icon-button,
.bare-button {
  display: grid;
  place-items: center;
  padding: 0;
  border: 0;
  color: var(--muted);
  background: transparent;
}

.icon-button {
  width: 34px;
  height: 34px;
  border-radius: 11px;
  transition: color 160ms ease, background 160ms ease;
}

.icon-button:hover {
  color: var(--text);
  background: rgba(255, 255, 255, 0.065);
}

.icon-button.compact {
  width: 27px;
  height: 27px;
}

.icon-button.compact svg {
  width: 16px;
  height: 16px;
}

.playlist-list {
  min-height: 0;
  overflow-y: auto;
  scrollbar-width: thin;
  scrollbar-color: #2e323a transparent;
}

.playlist-row {
  position: relative;
  display: flex;
  align-items: center;
}

.playlist-button {
  display: grid;
  grid-template-columns: 28px minmax(0, 1fr);
  align-items: center;
  gap: 9px;
  padding: 9px 33px 9px 10px;
  border-radius: 11px;
  color: var(--muted);
  transition: color 160ms ease, background 160ms ease;
}

.playlist-button:hover,
.playlist-button.is-active {
  color: var(--text);
  background: rgba(255, 255, 255, 0.045);
}

.playlist-cover {
  width: 28px;
  height: 28px;
  display: grid;
  place-items: center;
  border: 1px solid rgba(255, 255, 255, 0.08);
  border-radius: 8px;
  color: hsl(var(--playlist-hue), 78%, 70%);
  background: linear-gradient(145deg, hsla(var(--playlist-hue), 55%, 35%, 0.45), #181a20);
  font-size: 10px;
  font-weight: 720;
  text-transform: uppercase;
}

.playlist-label {
  min-width: 0;
}

.playlist-label strong,
.playlist-label small {
  display: block;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.playlist-label strong {
  font-size: 12px;
  font-weight: 550;
}

.playlist-label small {
  margin-top: 2px;
  color: var(--quiet);
  font-size: 9px;
}

.playlist-delete {
  position: absolute;
  right: 5px;
  width: 26px;
  height: 26px;
  opacity: 0;
}

.playlist-row:hover .playlist-delete,
.playlist-delete:focus-visible {
  opacity: 1;
}

.playlist-delete:hover {
  color: var(--danger);
}

.empty-playlists {
  margin: 8px 11px;
  color: var(--quiet);
  font-size: 11px;
  line-height: 1.5;
}

.source-credit {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin: 14px 8px 0;
  padding-top: 15px;
  border-top: 1px solid var(--line);
  color: var(--quiet);
  text-decoration: none;
  font-size: 10px;
  transition: color 160ms ease;
}

.source-credit:hover {
  color: var(--muted);
}

.source-credit svg {
  width: 13px;
  height: 13px;
}

.player-panel {
  position: relative;
  min-width: 0;
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 27px clamp(28px, 4vw, 56px) 28px;
  overflow: hidden;
}

.player-topline {
  width: 100%;
  display: flex;
  align-items: center;
  justify-content: space-between;
}

.mode-pill {
  max-width: 60%;
  overflow: hidden;
  padding: 7px 10px;
  border: 1px solid var(--line);
  border-radius: 999px;
  color: var(--muted);
  background: rgba(255, 255, 255, 0.025);
  font-size: 9px;
  font-weight: 680;
  letter-spacing: 0.09em;
  text-overflow: ellipsis;
  text-transform: uppercase;
  white-space: nowrap;
}

.preload-status {
  display: inline-flex;
  align-items: center;
  gap: 7px;
  color: var(--quiet);
  font-size: 9px;
}

.preload-status i {
  width: 5px;
  height: 5px;
  border-radius: 50%;
  background: var(--accent);
  box-shadow: 0 0 8px rgba(var(--accent-rgb), 0.6);
}

.cover-wrap {
  position: relative;
  width: min(270px, 56%);
  aspect-ratio: 1;
  flex: 0 1 auto;
  margin: clamp(19px, 4vh, 35px) 0 clamp(18px, 3vh, 27px);
  overflow: hidden;
  border: 1px solid rgba(255, 255, 255, 0.09);
  border-radius: 28px;
  background: #16191d;
  box-shadow: 0 25px 60px rgba(0, 0, 0, 0.34);
}

.cover-wrap::after {
  content: "";
  position: absolute;
  inset: 0;
  pointer-events: none;
  border-radius: inherit;
  box-shadow: inset 0 1px rgba(255, 255, 255, 0.14);
}

.cover-fallback {
  position: absolute;
  inset: 0;
  display: grid;
  place-items: center;
  overflow: hidden;
  background:
    radial-gradient(circle at 30% 28%, hsla(var(--cover-hue), 90%, 68%, 0.33), transparent 29%),
    radial-gradient(circle at 75% 80%, hsla(calc(var(--cover-hue) + 65), 75%, 58%, 0.18), transparent 37%),
    linear-gradient(145deg, hsl(var(--cover-hue), 24%, 16%), #101217 62%);
  transition: background 600ms ease;
}

.cover-orbit {
  position: absolute;
  width: 73%;
  aspect-ratio: 1;
  border: 1px solid hsla(var(--cover-hue), 70%, 72%, 0.2);
  border-radius: 50%;
  transform: rotate(-24deg) scaleY(0.48);
}

.cover-orbit::before,
.cover-orbit::after {
  content: "";
  position: absolute;
  border-radius: 50%;
}

.cover-orbit::before {
  width: 8px;
  height: 8px;
  top: 47%;
  left: -4px;
  background: hsl(var(--cover-hue), 80%, 70%);
  box-shadow: 0 0 20px hsla(var(--cover-hue), 90%, 60%, 0.8);
}

.cover-orbit::after {
  inset: 27%;
  border: 1px solid hsla(var(--cover-hue), 90%, 88%, 0.09);
}

.cover-letter {
  color: hsla(var(--cover-hue), 80%, 87%, 0.88);
  font-family: Georgia, serif;
  font-size: clamp(64px, 7vw, 96px);
  font-style: italic;
  font-weight: 700;
  text-shadow: 0 10px 40px hsla(var(--cover-hue), 80%, 50%, 0.2);
}

#coverImage {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  display: block;
  object-fit: cover;
  opacity: 0;
  transform: scale(1.015);
  transition: opacity 300ms ease, transform 800ms ease;
}

#coverImage.is-loaded {
  opacity: 1;
  transform: scale(1);
}

.equalizer {
  position: absolute;
  right: 14px;
  bottom: 14px;
  width: 34px;
  height: 29px;
  display: flex;
  align-items: flex-end;
  justify-content: center;
  gap: 3px;
  padding: 7px;
  border: 1px solid rgba(255, 255, 255, 0.13);
  border-radius: 10px;
  background: rgba(8, 9, 11, 0.68);
  backdrop-filter: blur(10px);
  opacity: 0;
  transition: opacity 160ms ease;
}

.is-playing .equalizer {
  opacity: 1;
}

.equalizer i {
  width: 2px;
  height: 4px;
  border-radius: 2px;
  background: var(--accent);
  animation: equalize 650ms ease-in-out infinite alternate;
}

.equalizer i:nth-child(2) { animation-delay: -380ms; }
.equalizer i:nth-child(3) { animation-delay: -120ms; }
.equalizer i:nth-child(4) { animation-delay: -500ms; }

@keyframes equalize {
  to { height: 14px; }
}

.track-copy {
  width: 100%;
  min-width: 0;
  text-align: center;
}

.eyebrow {
  margin: 0 0 7px;
  color: var(--quiet);
  font-size: 9px;
  font-weight: 680;
  letter-spacing: 0.13em;
  text-transform: uppercase;
}

.track-copy h1 {
  max-width: 100%;
  margin: 0;
  overflow: hidden;
  font-size: clamp(22px, 3vw, 29px);
  font-weight: 680;
  letter-spacing: -0.035em;
  line-height: 1.15;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.track-copy > p:last-child {
  max-width: 92%;
  margin: 7px auto 0;
  overflow: hidden;
  color: var(--muted);
  font-size: 11px;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.timeline {
  width: 100%;
  margin-top: clamp(18px, 3vh, 26px);
}

input[type="range"] {
  width: 100%;
  height: 3px;
  margin: 0;
  border-radius: 999px;
  outline: none;
  appearance: none;
  background: linear-gradient(
    to right,
    var(--accent) 0 var(--range-progress, 0%),
    #30333a var(--range-progress, 0%) 100%
  );
}

input[type="range"]::-webkit-slider-thumb {
  width: 12px;
  height: 12px;
  border: 0;
  border-radius: 50%;
  appearance: none;
  background: var(--text);
  box-shadow: 0 1px 6px rgba(0, 0, 0, 0.45);
}

input[type="range"]::-moz-range-thumb {
  width: 12px;
  height: 12px;
  border: 0;
  border-radius: 50%;
  background: var(--text);
  box-shadow: 0 1px 6px rgba(0, 0, 0, 0.45);
}

.time-row {
  display: flex;
  justify-content: space-between;
  margin-top: 7px;
  color: var(--quiet);
  font-size: 9px;
  font-variant-numeric: tabular-nums;
}

.transport {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 20px;
  margin-top: 11px;
}

.round-button {
  display: grid;
  place-items: center;
  padding: 0;
  border: 0;
  border-radius: 50%;
  transition: color 160ms ease, background 160ms ease, transform 160ms ease;
}

.round-button:hover {
  transform: translateY(-1px);
}

.round-button:active {
  transform: scale(0.96);
}

.round-button.secondary {
  width: 40px;
  height: 40px;
  color: var(--muted);
  background: transparent;
}

.round-button.secondary:hover {
  color: var(--text);
  background: rgba(255, 255, 255, 0.055);
}

.play-button {
  width: 56px;
  height: 56px;
  color: #0d1009;
  background: var(--accent);
  box-shadow: 0 10px 30px rgba(var(--accent-rgb), 0.18);
}

.play-button svg {
  width: 21px;
  height: 21px;
  stroke-width: 2;
}

.play-button .pause-icon,
.is-playing .play-button .play-icon {
  display: none;
}

.is-playing .play-button .pause-icon {
  display: block;
}

.player-actions {
  width: 100%;
  display: grid;
  grid-template-columns: 1fr auto 1fr;
  align-items: center;
  margin-top: auto;
  padding-top: 19px;
}

.soft-button,
.primary-button,
.outline-button {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: 8px;
  border-radius: 11px;
  font-weight: 620;
}

.soft-button {
  justify-self: start;
  padding: 9px 12px;
  border: 1px solid var(--line);
  color: var(--muted);
  background: rgba(255, 255, 255, 0.025);
  font-size: 10px;
  transition: color 160ms ease, background 160ms ease;
}

.soft-button:hover {
  color: var(--text);
  background: rgba(255, 255, 255, 0.055);
}

.soft-button svg {
  width: 15px;
  height: 15px;
}

.volume-control {
  width: 112px;
  display: flex;
  align-items: center;
  gap: 8px;
}

.volume-control .bare-button,
.source-link {
  width: 29px;
  height: 29px;
  border-radius: 9px;
}

.volume-control .bare-button:hover,
.source-link:hover {
  color: var(--text);
  background: rgba(255, 255, 255, 0.05);
}

.volume-control svg,
.source-link svg {
  width: 15px;
  height: 15px;
}

#volume {
  height: 2px;
}

#volume::-webkit-slider-thumb {
  width: 9px;
  height: 9px;
}

.muted-icon,
.is-muted .volume-icon {
  display: none;
}

.is-muted .muted-icon {
  display: block;
}

.source-link {
  justify-self: end;
  color: var(--muted);
}

.source-link.is-disabled {
  pointer-events: none;
  opacity: 0.35;
}

.loading-layer {
  position: absolute;
  inset: 0;
  z-index: 4;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 15px;
  background: rgba(17, 19, 23, 0.93);
  backdrop-filter: blur(18px);
  opacity: 0;
  pointer-events: none;
  transition: opacity 220ms ease;
}

.loading-layer.is-visible {
  opacity: 1;
  pointer-events: auto;
}

.loading-layer p {
  margin: 0;
  color: var(--muted);
  font-size: 11px;
}

.loader {
  width: 30px;
  height: 30px;
  border: 2px solid rgba(255, 255, 255, 0.08);
  border-top-color: var(--accent);
  border-radius: 50%;
  animation: spin 750ms linear infinite;
}

@keyframes spin { to { transform: rotate(360deg); } }

.queue-panel {
  display: flex;
  flex-direction: column;
  padding: 27px 18px 18px;
  border-left: 1px solid var(--line);
}

.queue-tabs {
  display: flex;
  gap: 5px;
  padding: 3px;
  border: 1px solid var(--line);
  border-radius: 12px;
  background: rgba(255, 255, 255, 0.018);
}

.queue-tab {
  flex: 1;
  padding: 8px;
  border: 0;
  border-radius: 9px;
  color: var(--quiet);
  background: transparent;
  font-size: 10px;
  font-weight: 620;
  transition: color 160ms ease, background 160ms ease;
}

.queue-tab:hover {
  color: var(--muted);
}

.queue-tab.is-active {
  color: var(--text);
  background: #23262c;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.22);
}

.queue-context {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  padding: 22px 5px 11px;
  color: var(--muted);
  font-size: 10px;
}

.queue-context span:first-child {
  overflow: hidden;
  font-weight: 620;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.queue-context span:last-child {
  flex: none;
  color: var(--quiet);
  font-size: 9px;
}

.track-list {
  min-height: 0;
  overflow-y: auto;
  scrollbar-width: thin;
  scrollbar-color: #2e323a transparent;
}

.track-row {
  position: relative;
  display: grid;
  grid-template-columns: 38px minmax(0, 1fr) 24px;
  align-items: center;
  gap: 10px;
  width: 100%;
  padding: 8px 5px;
  border: 0;
  border-radius: 11px;
  color: var(--muted);
  text-align: left;
  background: transparent;
  transition: background 150ms ease;
}

.track-row:hover,
.track-row.is-current {
  background: rgba(255, 255, 255, 0.043);
}

.track-mini-cover {
  position: relative;
  width: 38px;
  height: 38px;
  display: grid;
  place-items: center;
  overflow: hidden;
  border-radius: 10px;
  color: hsl(var(--track-hue), 80%, 74%);
  background:
    radial-gradient(circle at 28% 25%, hsla(var(--track-hue), 85%, 65%, 0.35), transparent 35%),
    linear-gradient(145deg, hsl(var(--track-hue), 25%, 18%), #15171c);
  font-family: Georgia, serif;
  font-size: 15px;
  font-style: italic;
  font-weight: 700;
}

.track-mini-cover img {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.track-row-copy {
  min-width: 0;
}

.track-row-copy strong,
.track-row-copy small {
  display: block;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.track-row-copy strong {
  color: #d9d8d3;
  font-size: 11px;
  font-weight: 570;
}

.track-row.is-current .track-row-copy strong {
  color: var(--accent);
}

.track-row-copy small {
  margin-top: 3px;
  color: var(--quiet);
  font-size: 9px;
}

.track-index {
  justify-self: center;
  color: var(--quiet);
  font-size: 9px;
  font-variant-numeric: tabular-nums;
}

.track-remove {
  width: 24px;
  height: 24px;
  opacity: 0;
}

.track-row:hover .track-remove,
.track-remove:focus-visible {
  opacity: 1;
}

.track-remove:hover {
  color: var(--danger);
}

.queue-empty {
  min-height: 180px;
  display: none;
  flex: 1;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 9px;
  color: var(--quiet);
  text-align: center;
}

.queue-empty.is-visible {
  display: flex;
}

.queue-empty svg {
  width: 24px;
  height: 24px;
  opacity: 0.55;
}

.queue-empty p {
  max-width: 150px;
  margin: 0;
  font-size: 10px;
  line-height: 1.5;
}

.dialog {
  width: min(390px, calc(100vw - 32px));
  padding: 22px;
  border: 1px solid rgba(255, 255, 255, 0.11);
  border-radius: 20px;
  color: var(--text);
  background: #181a1f;
  box-shadow: 0 30px 90px rgba(0, 0, 0, 0.66);
}

.dialog::backdrop {
  background: rgba(3, 4, 5, 0.7);
  backdrop-filter: blur(8px);
}

.dialog-title {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 16px;
  margin-bottom: 22px;
}

.dialog-title small {
  color: var(--quiet);
  font-size: 9px;
  font-weight: 680;
  letter-spacing: 0.1em;
  text-transform: uppercase;
}

.dialog-title h2 {
  margin: 5px 0 0;
  font-size: 20px;
  letter-spacing: -0.025em;
}

.dialog label span {
  display: block;
  margin: 0 0 7px 2px;
  color: var(--muted);
  font-size: 10px;
}

.dialog input {
  width: 100%;
  padding: 12px 13px;
  border: 1px solid var(--line);
  border-radius: 11px;
  outline: none;
  color: var(--text);
  background: #111318;
  font-size: 12px;
  transition: border-color 160ms ease, box-shadow 160ms ease;
}

.dialog input:focus {
  border-color: rgba(var(--accent-rgb), 0.6);
  box-shadow: 0 0 0 3px rgba(var(--accent-rgb), 0.08);
}

.primary-button,
.outline-button {
  width: 100%;
  padding: 11px 14px;
  font-size: 11px;
}

.primary-button {
  margin-top: 14px;
  border: 0;
  color: #11130d;
  background: var(--accent);
}

.outline-button {
  border: 1px solid var(--line);
  color: var(--muted);
  background: transparent;
}

.outline-button:hover {
  color: var(--text);
  background: rgba(255, 255, 255, 0.045);
}

.outline-button svg {
  width: 15px;
  height: 15px;
}

.dialog-playlists {
  max-height: 240px;
  margin: -6px 0 12px;
  overflow-y: auto;
}

.dialog-playlist-button {
  width: 100%;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 15px;
  padding: 11px 10px;
  border: 0;
  border-radius: 10px;
  color: var(--muted);
  text-align: left;
  background: transparent;
}

.dialog-playlist-button:hover {
  color: var(--text);
  background: rgba(255, 255, 255, 0.045);
}

.dialog-playlist-button strong {
  overflow: hidden;
  font-size: 11px;
  font-weight: 560;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.dialog-playlist-button small {
  flex: none;
  color: var(--quiet);
  font-size: 9px;
}

.toast {
  position: fixed;
  z-index: 20;
  left: 50%;
  bottom: 25px;
  max-width: min(420px, calc(100vw - 32px));
  padding: 10px 14px;
  border: 1px solid rgba(255, 255, 255, 0.09);
  border-radius: 11px;
  color: var(--text);
  background: #21242a;
  box-shadow: 0 12px 36px rgba(0, 0, 0, 0.45);
  font-size: 10px;
  opacity: 0;
  pointer-events: none;
  transform: translate(-50%, 12px);
  transition: opacity 180ms ease, transform 180ms ease;
}

.toast.is-visible {
  opacity: 1;
  transform: translate(-50%, 0);
}

:focus-visible {
  outline: 2px solid rgba(var(--accent-rgb), 0.75);
  outline-offset: 2px;
}

@media (max-width: 940px) {
  body {
    display: block;
  }

  .app-shell {
    width: 100%;
    height: 100vh;
    min-height: 650px;
    grid-template-columns: 76px minmax(360px, 1fr) 270px;
    border: 0;
    border-radius: 0;
  }

  .sidebar {
    align-items: center;
    padding-inline: 10px;
  }

  .brand {
    margin-inline: 0;
  }

  .brand > span:last-child,
  .source-button > span:nth-child(2),
  .source-button .active-dot,
  .section-heading > span,
  .playlist-label,
  .source-credit,
  .empty-playlists {
    display: none;
  }

  .source-button,
  .playlist-button {
    display: grid;
    place-items: center;
    padding: 7px;
  }

  .playlist-button {
    grid-template-columns: 1fr;
  }

  .playlist-delete {
    right: -4px;
    bottom: -3px;
    width: 20px;
    height: 20px;
    background: #202329;
  }

  .section-heading {
    justify-content: center;
    padding-inline: 0;
  }
}

@media (max-width: 760px) {
  body {
    overflow: auto;
  }

  .app-shell {
    height: auto;
    min-height: 100vh;
    grid-template-columns: 1fr;
    grid-template-rows: auto auto auto;
    overflow: visible;
  }

  .sidebar {
    position: sticky;
    top: 0;
    z-index: 5;
    height: 68px;
    display: grid;
    grid-template-columns: auto 1fr auto;
    padding: 10px 14px;
    border-right: 0;
    border-bottom: 1px solid var(--line);
    backdrop-filter: blur(20px);
  }

  .brand {
    margin: 0;
  }

  .source-nav {
    justify-self: end;
    margin: 0 8px 0 0;
  }

  .source-icon {
    width: 34px;
    height: 34px;
  }

  .playlists-section {
    min-height: auto;
    display: block;
  }

  .section-heading {
    position: absolute;
    right: 14px;
    top: 20px;
    padding: 0;
  }

  .playlist-list,
  .source-credit,
  .empty-playlists {
    display: none;
  }

  .player-panel {
    min-height: 690px;
    padding: 24px clamp(24px, 8vw, 58px) 27px;
  }

  .cover-wrap {
    width: min(290px, 78%);
  }

  .queue-panel {
    min-height: 360px;
    padding: 22px 20px 28px;
    border-top: 1px solid var(--line);
    border-left: 0;
  }

  .track-list {
    max-height: 430px;
  }
}

@media (max-width: 430px) {
  .player-panel {
    min-height: 630px;
    padding-inline: 22px;
  }

  .cover-wrap {
    width: min(250px, 82%);
  }

  .preload-status span {
    display: none;
  }

  .player-actions {
    grid-template-columns: 1fr auto;
  }

  .volume-control {
    justify-self: end;
  }

  .source-link {
    display: none;
  }
}

@media (prefers-reduced-motion: reduce) {
  *,
  *::before,
  *::after {
    scroll-behavior: auto !important;
    animation-duration: 0.01ms !important;
    animation-iteration-count: 1 !important;
    transition-duration: 0.01ms !important;
  }
}
