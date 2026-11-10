const frames = new Map();
const edge = navigator.userAgentData?.brands?.some(({ brand }) => brand === "Microsoft Edge") ||
  navigator.userAgent.includes(" Edg/");
const browser = edge ? "msedge.exe" : "chrome.exe";
let host = null;
let heartbeat = null;

function tabs() {
  const byTab = new Map();
  for (const frame of frames.values()) {
    if (!frame.media) continue;
    const old = byTab.get(frame.id);
    if (!old || (frame.media.playing && !old.media.playing) ||
        (frame.media.playing === old.media.playing && frame.changed > old.changed)) {
      byTab.set(frame.id, frame);
    }
  }
  return byTab;
}

function publish() {
  const live = tabs();
  if (live.size === 0) {
    if (host) host.postMessage({ browser, tabs: [] });
    clearInterval(heartbeat);
    heartbeat = null;
    host?.disconnect();
    host = null;
    return;
  }
  if (!host) {
    host = chrome.runtime.connectNative("com.zenith_dot.browser");
    const connected = host;
    host.onDisconnect.addListener(() => {
      // Native messaging may be disabled or the companion may have exited.
      void chrome.runtime.lastError;
      if (host !== connected) return;
      host = null;
      clearInterval(heartbeat);
      heartbeat = null;
    });
    host.onMessage.addListener((command) => {
      if (!["toggle", "seek"].includes(command?.action)) return;
      const target = tabs().get(command.id);
      if (target) target.port.postMessage(command);
    });
    heartbeat = setInterval(publish, 5000);
  }
  const now = Date.now();
  const entries = [...live.values()].slice(0, 16).map(({ id, media, changed }) => ({
    id, ...media,
    position: Math.min(media.duration, media.position + (media.playing ? (now - changed) * media.rate / 1000 : 0))
  }));
  host.postMessage({ browser, tabs: entries });
}

chrome.runtime.onConnect.addListener((port) => {
  const id = port.sender?.tab?.id;
  if (port.name !== "zenith-media" || !Number.isInteger(id)) return;
  const frame = { id, port, media: null, changed: 0 };
  frames.set(port, frame);
  port.onMessage.addListener((media) => {
    frame.media = media;
    frame.changed = Date.now();
    publish();
  });
  port.onDisconnect.addListener(() => {
    frames.delete(port);
    publish();
  });
});
