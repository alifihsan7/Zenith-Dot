(() => {
  const events = ["play", "playing", "pause", "ended", "durationchange", "loadedmetadata", "emptied", "seeked", "ratechange"];
  let current = null;
  let port = null;
  let update = null;
  let lastSnapshot = "";
  let reportedWatchId = null;
  let skipButtons = [];
  let skipObserver = null;

  const youtube = location.hostname === "www.youtube.com" || location.hostname === "youtube.com";
  const skipSelectors = [".ytp-prev-button", ".ytp-next-button"];

  function youtubeWatchId() {
    if (!youtube || location.pathname !== "/watch") return null;
    return new URLSearchParams(location.search).get("v");
  }

  function youtubeWatch(videoId) {
    const watch = document.querySelector("ytd-watch-flexy[video-id]");
    if (watch?.getAttribute("video-id") !== videoId) return null;
    const player = watch.querySelector("#movie_player");
    const video = player?.querySelector("video.html5-main-video");
    const title = watch.querySelector("ytd-watch-metadata h1")?.textContent.trim();
    if (!video || !title || video.ended || video.readyState < 1) return null;
    const artist = watch.querySelector("ytd-watch-metadata #owner ytd-channel-name a")?.textContent.trim() || "";
    return { player, video, title, artist };
  }

  function available(button) {
    return Boolean(button?.isConnected && !button.matches('[disabled], [aria-disabled="true"], [hidden]') &&
      button.getClientRects().length &&
      (!button.matches("a") || (button.getAttribute("href") && button.getAttribute("href") !== "#")));
  }

  function observeSkipButtons(player) {
    const buttons = skipSelectors.map(selector => player?.querySelector(selector));
    if (buttons.every((button, index) => button === skipButtons[index])) return;
    skipButtons = buttons;
    skipObserver ??= new MutationObserver(() => schedule());
    skipObserver.disconnect();
    for (const button of buttons) {
      if (button) skipObserver.observe(button, {
        attributes: true, attributeFilter: ["aria-disabled", "disabled", "hidden", "href", "class", "style"]
      });
    }
  }

  function candidate() {
    const media = [...document.querySelectorAll("audio, video")];
    if (current && current.isConnected && !current.paused && !current.ended) return current;
    return media.find((item) => !item.paused && !item.ended && item.readyState >= 1) ??
      (current?.isConnected && !current.ended && current.readyState >= 1 ? current : null);
  }

  function send() {
    clearTimeout(update);
    update = null;
    const watchId = youtubeWatchId();
    const watch = watchId ? youtubeWatch(watchId) : null;
    if (youtube) observeSkipButtons(watch?.player);
    const video = watch?.video;
    if (watchId) {
      current = video && (!video.paused || video === current || video.currentTime > 0) ? video : null;
    } else {
      current = candidate();
    }
    if (!current) {
      if (port) port.disconnect();
      port = null;
      lastSnapshot = "";
      return;
    }

    const media = watch ? null : navigator.mediaSession?.metadata;
    const duration = Number.isFinite(current.duration) && current.duration > 0 ? current.duration : 0;
    const snapshot = {
      title: (watch?.title || media?.title || document.title).slice(0, 240),
      artist: (watch?.artist || media?.artist || "").slice(0, 240),
      album: (media?.album || "").slice(0, 240),
      position: duration ? current.currentTime : 0,
      duration,
      playing: !current.paused && !current.ended,
      canSeek: duration > 0 && current.seekable.length > 0,
      canPrevious: available(skipButtons[0]),
      canNext: available(skipButtons[1]),
      rate: current.playbackRate
    };
    const value = JSON.stringify(snapshot);
    if (value === lastSnapshot && watchId === reportedWatchId && port) return;
    if (!port) {
      const connected = chrome.runtime.connect({ name: "zenith-media" });
      port = connected;
      connected.onDisconnect.addListener(() => {
        if (port === connected) {
          port = null;
          lastSnapshot = "";
        }
      });
      connected.onMessage.addListener((command) => {
        const id = youtubeWatchId();
        const activeWatch = id ? youtubeWatch(id) : null;
        if (!current?.isConnected || id !== reportedWatchId || (id && activeWatch?.video !== current)) return;
        if (command.action === "toggle") {
          if (current.paused) void current.play().catch(() => {});
          else current.pause();
        } else if (command.action === "seek" && Number.isFinite(command.seconds) &&
                   Number.isFinite(current.duration) && current.seekable.length) {
          current.currentTime = Math.min(current.duration, Math.max(0, command.seconds));
        } else if (activeWatch && (command.action === "previous" || command.action === "next")) {
          const button = activeWatch.player.querySelector(skipSelectors[command.action === "previous" ? 0 : 1]);
          if (available(button)) button.click();
        }
      });
    }
    lastSnapshot = value;
    reportedWatchId = watchId;
    port.postMessage(snapshot);
  }

  function schedule(event) {
    if (!youtubeWatchId() && event?.target instanceof HTMLMediaElement && event.type === "play") current = event.target;
    if (update === null) update = setTimeout(send, 80);
  }

  for (const event of events) document.addEventListener(event, schedule, true);
  if (youtube) {
    document.addEventListener("yt-navigate-finish", schedule);
    document.addEventListener("yt-page-data-updated", schedule);
    const title = document.querySelector("head > title");
    if (title) new MutationObserver(() => schedule()).observe(title, { childList: true, characterData: true, subtree: true });
  }
  window.addEventListener("pagehide", () => {
    clearTimeout(update);
    update = null;
    if (port) port.disconnect();
    port = null;
    lastSnapshot = "";
  });
  window.addEventListener("pageshow", schedule);
  send();
})();
