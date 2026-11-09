# Zenith Dot

A Windows 11 overlay that sits as a dot at the top center of the primary display. When a Windows media session starts playing, the dot expands to show the track, playback controls, and progress. Click the dot to check battery status, or open the tray menu for media controls and Exit.

The app uses C++20, Win32, Direct2D, DirectWrite, and C++/WinRT. It runs as one desktop process. The frame timer stops when the dot settles. Windows media events drive session updates, and the app reads power status every ten seconds and when Windows reports a change.

## Build

Install Visual Studio 2022 or Build Tools with the **Desktop development with C++** workload, the Windows SDK, and CMake. Run these commands from the repository directory.

```powershell
cmake -S . -B out -G "Visual Studio 17 2022" -A x64
cmake --build out --config Release --parallel
cmake --install out --config Release --prefix dist
```

The [Windows workflow](.github/workflows/windows.yml) also builds a downloadable x64 artifact. Run `ZenithDot.exe` from the build package for a portable launch.

## Install and start at sign-in

In the extracted build package or `dist` directory, run `.\install.ps1` from PowerShell. The script installs for the current user and creates Start menu and Startup shortcuts. Open **Zenith Dot** from Start to launch immediately. To uninstall, exit from the tray menu and run `.\uninstall.ps1` from the same package.

## Source layout

| Module | Responsibility |
| --- | --- |
| `src/main.cpp` | Window lifecycle, single instance, tray, timers, and input |
| `src/island.*` | Transparent rendering, transitions, progress, and power display |
| `src/media.*` | Windows GSMTC sessions, metadata, timeline, and transport controls |
| `src/power.*` | Native battery and AC telemetry |
| `scripts/` | Per-user installation and removal |

## Verify on Windows 11

- Start Spotify or YouTube in a browser that publishes a Windows media session. Check track metadata and playback progress where the player provides them. Try Previous, Pause, and Next, then resume from the tray. If one player pauses while another is still playing, verify the island switches to the playing session.
- Click the idle dot to check battery percentage and AC status. Unplug and reconnect the charger. On a desktop without a battery, confirm the panel reports that state.
- Install, sign out, sign back in, and confirm there is one Zenith Dot process. Launch it again from Start and check that the process count stays at one.
- Compare private working set after a minute at idle and during playback. For an Electron baseline, include its renderer and helper processes.
