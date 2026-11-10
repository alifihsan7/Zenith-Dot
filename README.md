# Zenith Dot

A Windows 11 overlay that sits as a dot at the top center of the primary display. When a Windows media session plays, the dot grows into a pill with the track title and a waveform. After five seconds paused, it shrinks back to the dot. Hover the pill or the dot to open the panel:

- **Now Playing:** cover, title, artist, a seekable timeline, playback controls, and one button per app with a media session.
- **Focus:** a 25-minute timer. While it runs, the pill shows the time left.
- **System:** CPU, memory, and power status.

Short notices split off the pill as a liquid bubble, or grow out of the dot: plugging in or unplugging the charger, low battery at 20% and 10%, the end of a Focus session, and copying something (with a short preview). While the panel is open they show as a row at its top instead. Clipboard content is never stored, and content that password managers mark as private is not announced.

The accent colour follows the current cover. The tray menu has media controls and Exit.

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
| `src/island.*` | The dot, pill, and panel: rendering, animations, hover, and hit testing |
| `src/media.*` | Windows GSMTC sessions, metadata, timeline, and transport controls |
| `src/power.*` | Native battery and AC telemetry |
| `src/system.*` | CPU and memory use for the System screen |
| `scripts/` | Per-user installation and removal |

## Verify on Windows 11

- Start Spotify or YouTube in a browser that publishes a Windows media session. Check track metadata and playback progress where the player provides them. Try Previous, Pause, and Next, then resume from the tray. If one player pauses while another is still playing, verify the island switches to the playing session.
- Play tracks with differently coloured covers. The waveform and progress bar should fade to a colour from each cover, with a faint tint in the panel's lower-right corner. A greyscale cover or a player without artwork should fall back to light blue.
- Type in another app, then hover and click the island (including Previous, Pause, and Next). Focus and the caret must stay in that app. Right-click the island, close the menu, and confirm focus returns to the app.
- Hover the pill: the panel opens after a short delay, and the cover and title move into place. Move a little past the panel's edge and it should stay open, then close once the pointer is well away. Click outside it to close it right away.
- Switch between Now Playing, Focus, and System. Drag the timeline to seek where the player allows it. With two players open, click the other app's logo to show it.
- Pause playback: the pill should show a pause glyph, then shrink to the dot after five seconds.
- Copy some text: a "Copied" bubble should stretch out of the pill, pinch off, show the first line, and merge back after about two seconds. Copy with the panel open: the notice should appear as a row at the top of the panel. Copy a password from a password manager: no notice.
- Unplug and reconnect the charger: "On Battery" and then a green "Charging" bubble, each with the battery level.
- Open System and check the battery percentage and AC status. Unplug and reconnect the charger. On a desktop without a battery, confirm the panel reports that state.
- Install, sign out, sign back in, and confirm there is one Zenith Dot process. Launch it again from Start and check that the process count stays at one.
- Compare private working set after a minute at idle and during playback. For an Electron baseline, include its renderer and helper processes.
