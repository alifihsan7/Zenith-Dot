(() => {
  const events = ["play", "playing", "pause", "ended", "durationchange", "loadedmetadata", "emptied", "seeked", "ratechange"];
  let current = null;
  let port = null;
  let update = null;
  let lastSnapshot = "";

  function candidate() {
    const media = [...document.querySelectorAll("audio, video")];
    if (current && current.isConnected && !current.paused && !current.ended) return current;
    return media.find((item) => !item.paused && !item.ended && item.readyState >= 1) ??
      (current?.isConnected && !current.ended && current.readyState >= 1 ? current : null);
  }

  function send() {
    clearTimeout(update);
    update = null;
    current = candidate();
    if (!current) {
      if (port) port.disconnect();
      port = null;
      lastSnapshot = "";
      return;
    }

    const media = navigator.mediaSession?.metadata;
    const duration = Number.isFinite(current.duration) && current.duration > 0 ? current.duration : 0;
    const snapshot = {
      title: (media?.title || document.title).slice(0, 240),
      artist: (media?.artist || "").slice(0, 240),
      album: (media?.album || "").slice(0, 240),
      position: duration ? current.currentTime : 0,
      duration,
      playing: !current.paused && !current.ended,
      canSeek: duration > 0 && current.seekable.length > 0,
      rate: current.playbackRate
    };
    const value = JSON.stringify(snapshot);
    if (value === lastSnapshot && port) return;
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
        if (!current?.isConnected) return;
        if (command.action === "toggle") {
          if (current.paused) void current.play().catch(() => {});
          else current.pause();
        } else if (command.action === "seek" && Number.isFinite(command.seconds) &&
                   Number.isFinite(current.duration) && current.seekable.length) {
          current.currentTime = Math.min(current.duration, Math.max(0, command.seconds));
        }
      });
    }
    lastSnapshot = value;
    port.postMessage(snapshot);
  }

  function schedule(event) {
    if (event?.target instanceof HTMLMediaElement && event.type === "play") current = event.target;
    if (update === null) update = setTimeout(send, 80);
  }

  for (const event of events) document.addEventListener(event, schedule, true);
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
