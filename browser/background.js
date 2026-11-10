const frames = new Map();
const brands = navigator.userAgentData?.brands?.map(({ brand }) => brand) ?? [];
const browser = brands.includes("Microsoft Edge") || navigator.userAgent.includes(" Edg/") ? "msedge.exe"
  : brands.includes("Brave") || navigator.brave ? "brave.exe" : "chrome.exe";
let host = null;
let heartbeat = null;
let reconnect = null;
let download = null;
let downloadTimer = null;
let downloadRevision = 0;
const completed = new Map();
const completionHold = 9000;

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
  if (live.size === 0 && !download) {
    clearTimeout(reconnect);
    reconnect = null;
    if (host) host.postMessage({ browser, tabs: [] });
    clearInterval(heartbeat);
    heartbeat = null;
    host?.disconnect();
    host = null;
    return;
  }
  if (!host) {
    if (reconnect !== null) return;
    host = chrome.runtime.connectNative("com.zenith_dot.browser");
    const connected = host;
    host.onDisconnect.addListener(() => {
      // Native messaging may be disabled or the companion may have exited.
      void chrome.runtime.lastError;
      if (host !== connected) return;
      host = null;
      clearInterval(heartbeat);
      heartbeat = null;
      reconnect = setTimeout(() => {
        reconnect = null;
        publish();
      }, 5000);
    });
    host.onMessage.addListener((command) => {
      if (command?.action === "showDownload" && download?.complete && command.id === download.id) {
        chrome.downloads.show(command.id);
        return;
      }
      if (!["toggle", "seek", "activate"].includes(command?.action)) return;
      const target = tabs().get(command.id);
      if (!target) return;
      if (command.action === "activate") {
        chrome.tabs.update(target.id, { active: true })
          .then(tab => chrome.windows.update(tab.windowId, { focused: true }))
          .catch(() => {}); // The tab or window may have closed since the last snapshot.
      } else {
        target.port.postMessage(command);
      }
    });
    heartbeat = setInterval(publish, 5000);
  }
  const now = Date.now();
  const entries = [...live.values()].slice(0, 16).map(({ id, media, changed }) => ({
    id, ...media,
    position: Math.min(media.duration, media.position + (media.playing ? (now - changed) * media.rate / 1000 : 0))
  }));
  host.postMessage({ browser, tabs: entries, ...(download && { download }) });
}

function scheduleDownloads(delay = 0) {
  const revision = ++downloadRevision;
  clearTimeout(downloadTimer);
  downloadTimer = setTimeout(() => void refreshDownloads(revision), delay);
}

async function refreshDownloads(revision) {
  let active;
  try {
    active = (await chrome.downloads.search({ state: "in_progress" })).filter(item => !item.incognito);
  } catch {
    if (revision === downloadRevision && download) scheduleDownloads(5000);
    return;
  }
  if (revision !== downloadRevision) return;

  let next = null;
  if (active.length) {
    const total = active.reduce((sum, item) => sum + item.totalBytes, 0);
    const received = active.reduce((sum, item) => sum + item.bytesReceived, 0);
    const paused = active.every(item => item.paused);
    const known = total > 0 && active.every(item => item.totalBytes > 0);
    const filename = active.length === 1 ? active[0].filename.split(/[\\/]/).pop().slice(0, 180) : "";
    const progress = known ? Math.max(0, Math.min(99, Math.floor(100 * received / total))) : -1;
    next = { count: active.length, filename, paused, progress };
    if (!paused) scheduleDownloads(1000);
  } else if (completed.size) {
    let latestId = null;
    let latestTime = 0;
    for (const [id, time] of completed) {
      if (time >= latestTime) {
        latestId = id;
        latestTime = time;
      }
    }
    const remaining = latestTime + completionHold - Date.now();
    if (remaining > 0) {
      next = { count: completed.size, complete: true, id: latestId };
      scheduleDownloads(remaining);
    } else {
      completed.clear();
    }
  }
  if (JSON.stringify(next) !== JSON.stringify(download)) {
    download = next;
    publish();
  }
}

chrome.downloads.onCreated.addListener(item => {
  if (item.incognito) return;
  if (download?.complete) completed.clear();
  scheduleDownloads();
});
chrome.downloads.onChanged.addListener(delta => {
  if (delta.state?.current !== "complete") {
    if (delta.state || delta.paused || delta.filename || delta.totalBytes) scheduleDownloads();
    return;
  }
  clearTimeout(downloadTimer);
  ++downloadRevision;
  chrome.downloads.search({ id: delta.id }).then(([item]) => {
    if (item?.state === "complete" && !item.incognito && !completed.has(item.id)) {
      completed.set(item.id, Date.parse(item.endTime) || Date.now());
    }
    scheduleDownloads();
  }).catch(() => scheduleDownloads());
});
chrome.downloads.onErased.addListener(id => {
  completed.delete(id);
  scheduleDownloads();
});
chrome.runtime.onStartup.addListener(() => scheduleDownloads());
scheduleDownloads();

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
