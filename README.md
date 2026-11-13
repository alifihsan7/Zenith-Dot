# Zenula

A small native overlay for Windows 11 that sits at the top center of the primary display. It is a dot when idle, grows into a pill while music or a timer runs, and opens a panel on hover. Pin the panel to keep it open; when it closes, Zenula remembers the last page.

Built with C++20, Win32, Direct2D, DirectWrite, and C++/WinRT as one lightweight process. The frame timer stops whenever nothing moves.

## Features

- **Home:** 24-hour clock, a summary of running activities, quick actions, and Settings. **Auto-shrink** shrinks the pill when it covers detected window controls.
- **Now Playing:** cover, title, artist, timeline, playback controls, source picker, and a spectrum visualizer. Synced lyrics from [LRCLIB](https://lrclib.net) can show in the panel or the pill, with an adjustable offset.
- **Timers:** Focus, Break, and Quick Timer with adjustable lengths, plus an optional automatic Focus/Break cycle.
- **Scratchpad:** **Notes** for free text and **Tasks** for a checklist, both saved locally. Click outside the editor or press Esc to stop typing.
- **Drop Zone:** hold several files, text snippets, and screenshots, with image previews and drag-out.
- **System:** CPU, RAM, GPU, storage, battery, Wi-Fi, Bluetooth, the audio output picker, and per-app volume.
- **Prayer:** official SIHAT Kemenag RI schedules for 517 regencies and cities, a searchable location picker, and reminders on time or a few minutes early.

When music and a timer run together, one shows as the pill and the other as a companion dot; click the dot to swap them. Short notices (clipboard, charger, low battery, connection, Caps Lock/Num Lock, Bluetooth) split off the pill as a liquid bubble. Camera, microphone, and screen sharing show as status icons. Zenula supports Do Not Disturb and can hide during fullscreen. The accent color follows the current cover.

## Build

Install Visual Studio 2022 (or Build Tools) with **Desktop development with C++**, the Windows SDK, and CMake. From the repository directory:

```powershell
cmake -S . -B out -G "Visual Studio 17 2022" -A x64
cmake --build out --config Release --parallel
cmake --install out --config Release --prefix dist
```

## Install

In `dist` (or an extracted build package), run `.\install.ps1`. It installs for the current user and adds Start menu and Startup shortcuts. To remove Zenula, exit from the tray and run `.\uninstall.ps1`.

Open Settings from the tray, from Home, or with `Zenula.exe --settings`. Notes and tasks are stored in `%LOCALAPPDATA%\Zenith Dot\`.

## Optional extras

- **Browser tabs:** Chrome, Edge, and Brave share one media session. To control individual tabs, run `.\enable-browser-tabs.ps1` from the package, then load the `browser` folder as an unpacked extension.
- **Windows app notifications:** the MSIX build can preview incoming Windows notifications. Build it with `scripts\package-msix.ps1` and enable **Connect Windows apps** in Settings.

## Source layout

| Path | Purpose |
| --- | --- |
| `src/island.*` | Dot, pill, panels, animations, and hit testing |
| `src/main.cpp` | Window, tray, timers, input, and Windows services |
| `src/settings_window.cpp` | The separate Settings window |
| `src/media.*`, `src/lyrics.*`, `src/audio_spectrum.*` | Media sessions, lyrics, and the visualizer |
| `src/scratchpad.*`, `src/tasks.*`, `src/drop_zone.*` | Notes, tasks, and Drop Zone |
| `src/system.*`, `src/power.*`, `src/desktop.*`, `src/bluetooth.*` | System status and device monitoring |
| `src/prayer*` | Prayer schedules and the location picker |
| `browser/`, `scripts/` | Browser extension, install, and packaging scripts |
