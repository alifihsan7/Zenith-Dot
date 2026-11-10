# Zenith Dot

A Windows 11 overlay that sits as a dot at the top center of the primary display. When a Windows media session plays, the dot grows into a pill with the track title and a waveform. After five seconds paused, it shrinks back to the dot. Hover the pill or the dot to open the panel:

- **Now Playing:** cover, title, artist, a seekable timeline, playback controls, and one button per app with a media session. Open **Lyrics** with the music note beside the title.
- **Timers:** choose Focus, Break, or Quick Timer from one tab. Focus and Break start at 25 and 5 minutes. Quick Timer offers 1, 5, and 10 minute presets, or use − and + to set 1–120 minutes. It can run alongside a Focus or Break countdown. The pill shows the selected active timer.
- **Scratchpad:** one note that saves locally as you type. While editing, press Esc to close the panel and return focus to the previous app.
- **Drop Zone.** Drop files from Explorer or text from another app onto the island. Each file gets its own row, and new drops join the existing items. Drag a row to another app, use × to remove it, or choose **Clear all**. The list holds up to 64 items in memory until Zenith Dot exits.
- **System:** CPU, memory, power, audio output, and Windows notification mode. Click the output name to select a device. The picker changes the default for system and media audio and also offers a shortcut to Sound settings. The notification row opens Windows notification settings.

Click the pin icon beside the panel tabs to keep Zenith Dot open while you use another app. The filled icon indicates that the panel is pinned. Click it again to restore hover dismissal. When the panel closes, the next hover restores the last tab and any open Lyrics column. If you scrolled manually, the reading position is kept for the same track.

Scroll over the compact pill or anywhere in the open panel to move one tab at a time. Over the pill, the new tab's icon and name appear briefly before the panel opens. Lyrics, the Scratchpad editor, and a Drop Zone list with more than four rows use the wheel to scroll their own content.

Short notices split off the pill as a liquid bubble, or grow out of the dot: plugging in or unplugging the charger, low battery at 20% and 10%, the end of a Focus session, and copying something (with a short preview). While the panel is open they show as a row at its top instead. Clipboard content is never stored, and content that password managers mark as private is not announced.

When the panel closes, it shrinks to a pill. The pill stays visible while an activity is running and shrinks to the black dot when idle.

Zenith Dot hides during fullscreen on the primary display and resumes when you leave fullscreen. Microphone or camera activity shows a pill with a device icon and label. When music or a timer is active, device icons sit beside that activity. Orange marks the microphone, and green marks the camera. Both icons appear when both devices are in use. The panel shows the same icons beside its tabs. A microphone muted at the Windows device level is excluded. An app's own mute button can leave its Windows capture session active.

System volume and brightness changes show a meter that updates in place as you adjust the level. Brightness requires a display that publishes Windows WMI brightness events. The Windows volume and brightness flyouts can appear alongside Zenith Dot.

The accent colour follows the current cover. The tray menu has media controls, **Hide Zenith Dot**, and **Exit**. Hiding leaves playback running. Choose **Show Zenith Dot** from the tray to bring it back.

Zenith bubbles, including the live clipboard preview, still appear when Windows Do Not Disturb is on. System shows the current Windows notification mode. Windows builds without the notification-mode API show **Notification status unavailable** in System.

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

The Scratchpad note is stored in `%LOCALAPPDATA%\Zenith Dot\scratchpad.txt`.

## Choose between browser tabs

Chrome, Edge, and Brave publish a shared Windows media session for ordinary tabs. Use the optional **Zenith Dot Browser Tabs** extension in the build package for individual tab controls and to keep YouTube watch-page titles tied to the main player.

1. Install the current Zenith Dot build and run `.\enable-browser-tabs.ps1` from the package. To use a portable build, pass its executable via `-Executable` and run that same copy of Zenith Dot.
2. Open `chrome://extensions`, `edge://extensions`, or `brave://extensions`. Turn on Developer mode, choose **Load unpacked**, and select the package's `browser` folder. If the extension is already installed from another folder, replace it with the copy from this package.
3. Reload existing media tabs. Play two HTML media streams and hover Zenith Dot. Click a numbered browser logo to select that tab, then use play/pause or the seek bar where the media supports it.

The extension sends playback metadata and timing to the local native messaging host. It starts the companion when it has media to report. Previous and Next are available through Windows media sessions. Browser tab controls use the page's HTML audio or video element. If Windows reports different artwork metadata for a browser tab, Zenith Dot shows the browser icon until the track details match.

## Lyrics

Opening **Lyrics** sends the selected track's title, artist, duration, and album when available to [LRCLIB](https://lrclib.net/docs). When a matching track has synced lyrics and the player supplies a timeline, the side column shows the full lyrics within the height of Now Playing. Long lines wrap, and the active line uses a brighter text colour. Scroll over the lyrics to read ahead or back. Playback following resumes five seconds after the last scroll. If the player supports seeking, click any lyric line to jump to its timestamp.

With Lyrics enabled, the compact pill shows the current synced line beside the Lyrics icon. It widens to fit the text and wraps longer lines. New lines slide in as the old line fades, with resizing timed to make room for the text. The track title returns before the first line or at gaps marked in the lyrics. After a pause, the pill keeps the last line for five seconds before shrinking. If Focus, Quick Timer, and media overlap, the pill prioritizes the active feature you selected most recently.

If the first lookup misses, Zenith Dot searches for the same title and artist within four seconds of the track's duration. When only plain lyrics are available, the column shows them with an **Unsynced** label. If no lyrics are found, reopen Lyrics after 30 seconds to retry.

Temporary connection or service failures are retried automatically. If the lookup still fails, choose **Retry** in Lyrics to try again for the current track.

## Source layout

| Module | Responsibility |
| --- | --- |
| `src/main.cpp` | Window lifecycle, single instance, tray, timers, and input |
| `src/island.*` | The dot, pill, and panel: rendering, animations, hover, and hit testing |
| `src/media.*` | Windows GSMTC sessions, metadata, timeline, and transport controls |
| `src/lyrics.*` | On-demand lookup and parsing for synced lyrics |
| `src/power.*` | Native battery and AC telemetry |
| `src/system.*` | CPU and memory use for the System screen |
| `src/desktop.*` | Fullscreen detection, capture indicators, and Windows volume and brightness events |
| `src/scratchpad.*` | Native text editor, focus handoff, and local note storage |
| `src/browser_tabs.*` and `browser/` | Optional Chrome, Edge, and Brave tab detection and native messaging |
| `scripts/` | Per-user installation and removal |

## Verify on Windows 11

- Start Spotify or YouTube in a browser that publishes a Windows media session. Check track metadata and playback progress where the player provides them. Try Previous, Pause, and Next, then resume from the tray. If one player pauses while another is still playing, verify the island switches to the playing session.
- Switch tracks in Spotify and return to a recent track. Its cached cover should appear again while Windows refreshes the thumbnail. Verify that a new track never displays the previous track's cover.
- With the browser extension enabled, pause a YouTube video and hover a recommendation. The pill and panel should keep the main video's title and timeline. Then navigate to another video in the same tab and check that both update together.
- Play tracks with differently coloured covers. The waveform and progress bar should fade to a colour from each cover, with a faint tint in the panel's lower-right corner. A greyscale cover or a player without artwork should fall back to light blue.
- Type in another app, then hover and click the island (including Previous, Pause, and Next). Focus and the caret must stay in that app. Right-click the island, close the menu, and confirm focus returns to the app.
- Hover the pill: the panel opens after a short delay. The title moves into place, while the cover fades in and out at its fixed position inside the panel. Move a little past the panel's edge and it should stay open, then close once the pointer is well away. Click outside it to close it right away.
- Switch between Now Playing, Timers, and System. Drag the timeline to seek where the player allows it. With two players open, click the other app's logo to show it.
- Open Timers, adjust Focus and Break, then switch to Quick Timer while Focus is running and start a second countdown. Check each timer's pill, pause/resume, and completion bubble. Close and reopen the panel and check that the last timer view returns.
- Open System and switch the Windows default audio output. Check the displayed name and the Sound Settings link. Turn on Windows Do Not Disturb, check the notification row, then copy ordinary text and verify that the **Copied** bubble still appears.
- Open Scratchpad, click the pin icon, and type a note. Switch to another app and confirm Scratchpad stays visible. Switch to Focus and System and check that the pin stays on. Unpin, close the panel, and hover again to check that System returns. Repeat with Lyrics open and manually scrolled. Reopen the editor, press Esc, and check that focus returns to the previous app. Restart Zenith Dot and confirm the note reloads.
- While music plays, choose **Hide Zenith Dot** from the tray. Confirm playback continues, then restore the island with **Show Zenith Dot**.
- With the browser extension enabled, play two tabs in the same browser and select each numbered logo. Verify that play/pause and seek affect the selected tab.
- Open Lyrics on a song with an LRCLIB match. Check that long lines wrap. Click a wrapped line and confirm playback jumps to its timestamp in a seekable player. Scroll to the first and last lines and check that playback following resumes five seconds after the last scroll. Switch tracks while a lookup is pending and check that the old track's lyrics disappear.
- Leave Lyrics enabled and move away from the panel. Check that the pill shows the active line, wraps a long line, and shrinks after five seconds paused. Trigger a Copy notice while the lyric pill is wide and verify that it splits off cleanly.
- Scroll over the compact pill and over the tab bar. Check the selected tab, the compact preview, and that scrolling inside Lyrics still moves the text.
- Pause playback: the pill should show a pause glyph, then shrink to the dot after five seconds.
- Copy some text: a "Copied" bubble should stretch out of the pill, pinch off, show the first line, and merge back after about two seconds. Copy with the panel open: the notice should appear as a row at the top of the panel. Copy a password from a password manager: no notice.
- Unplug and reconnect the charger: "On Battery" and then a green "Charging" bubble, each with the battery level.
- Open System and check the battery percentage and AC status. Unplug and reconnect the charger. On a desktop without a battery, confirm the panel reports that state.
- Enter and leave fullscreen in a video player on the primary display. The island should fade out, close its panel, and return after you exit fullscreen. Notices raised during fullscreen should expire without appearing afterwards.
- Start microphone and camera capture in an app. Check that both icons appear. With capture still open, mute the microphone in Windows and verify that the orange icon disappears and the green one remains. Unmute and verify that the orange icon returns. With two active microphones, muting one should leave the icon visible.
- Change volume repeatedly, then mute. Check that one volume notice updates in place. On a display that publishes Windows brightness events, repeat with brightness.
- Install, sign out, sign back in, and confirm there is one Zenith Dot process. Launch it again from Start and check that the process count stays at one.
- Compare private working set after a minute at idle and during playback. For an Electron baseline, include its renderer and helper processes.
