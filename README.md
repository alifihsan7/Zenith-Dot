# Zenith Dot

A Windows 11 overlay that sits as a dot at the top center of the primary display. The media pill shows the cover and grows to fit the track title, wrapping it when space is tight. Three seconds after playback pauses, the pill shrinks to the cover and pause icon. A music note stands in for missing artwork. Hover the pill or the dot to open the panel:

- **Now Playing:** cover, title, artist, a seekable timeline, and playback controls. Click the cover to return to the media app. Click the source logo beside **Lyrics** to switch between apps or browser tabs. Your selected source stays visible while its session is open, even if another player is running. With no media session, the shorter **Not Playing** panel keeps the feature tabs available.
- **Timers:** choose Focus, Break, or Quick Timer from one tab. Focus and Break start at 25 and 5 minutes. Quick Timer offers 1, 5, and 10 minute presets, or use − and + to set 1–120 minutes. It can run alongside a Focus or Break countdown. The pill shows the selected active timer. Turn on **Auto** to let Focus and Break follow each other without a click. A round has four Focus sessions, shown as dots beside **Auto**. The fourth break is three times the Break length, then the round starts again. The pill adds the round, such as **Focus 2/4**, and each hand-off shows the next phase in its notice. **Reset** returns to the first Focus. Zenith Dot remembers the Auto setting.
- **Scratchpad:** one note that saves locally as you type. While editing, press Esc to close the panel and return focus to the previous app.
- **Drop Zone.** Drop files or text onto the dashed area. Screenshots and other images copied on their own also appear in the list with a thumbnail. Click a thumbnail to preview the image in Zenith and close it with × or Esc to return to Drop Zone. Drag the rest of the row to another app. Use × beside an item to remove it or **Clear all** to empty the list. Up to 64 items can share the list. Clipboard images are exported as temporary PNGs that Zenith removes when you clear them or exit.
- **System:** CPU, memory, power, and Windows notification mode on the left, sound on the right. Click the output name to select a device. The picker changes the default for system and media audio and also offers a shortcut to Sound settings. Below it, **App Volume** lists each app with audio on the current output, like the Windows volume mixer. Drag a slider to set that app's volume, or click its icon to mute it. Three apps are visible at a time; scroll the list for more. The notification button opens Windows notification settings.

Click the pin icon beside the panel tabs to keep Zenith Dot open while you use another app. The filled icon indicates that the panel is pinned. Click it again to restore hover dismissal. When the panel closes, the next hover restores the last tab and any open Lyrics column. If you scrolled manually, the reading position is kept for the same track.

Scroll over the compact pill or anywhere in the open panel to move one tab at a time. Over the pill, the new tab's icon and name appear briefly before the panel opens. Lyrics, the Scratchpad editor, and a Drop Zone list with more than four rows use the wheel to scroll their own content.

Short notices split off the pill as a liquid bubble, or grow out of the dot: plugging in or unplugging the charger, low battery at 20% and 10%, the end of a Focus session, and copying something (with a short preview). While the panel is open they show as a row at its top instead. Zenith ignores clipboard content marked as private by the source app.

Open **Zenith Dot Settings** from the Start menu or choose **Settings** from the tray. From a portable build, run `ZenithDot.exe --settings`. **Appearance** controls the island's scale, minimum media pill width, accent, and fullscreen behavior. Island transitions follow Windows’ animation accessibility setting. **Features** lets you show, hide, and reorder the five feature tabs. Now Playing controls the waveform, pill lyrics, and compact pause delay. Timers controls the default Focus and Break durations. Drop Zone controls what is captured from the clipboard.

Click **Test** beside an appearance or feature setting to see a sample in the island at the top of the screen. The island returns to its live state automatically. Clipboard and device indicator tests use sample data.

**Add copied text to Drop Zone** is off by default. When enabled, the full text is available to drag out of Drop Zone until you remove it, clear the list, or exit. **Auto-add screenshots** stages clipboard images and is on by default. In **Privacy**, you can turn off the Copied bubble or its text snippet independently of Drop Zone staging. The camera and microphone indicators each have their own switch.

When the panel closes, it shrinks to a pill. The pill stays visible while an activity is running and shrinks to the black dot when idle.

The dot, pill, panel, and split bubbles use spring motion. Hover back while the panel closes to reverse its movement.

Zenith Dot hides during fullscreen on the primary display and resumes when you leave fullscreen. Microphone or camera activity shows a pill with a device icon and label. When music or a timer is active, device icons sit beside that activity. Orange marks the microphone, and green marks the camera. Both icons appear when both devices are in use. The panel shows the same icons beside its tabs. A microphone muted at the Windows device level is excluded. An app's own mute button can leave its Windows capture session active.

When a paired Bluetooth device connects or disconnects, a notice shows its name. Headsets and speakers use a headphone icon, and other devices use the Bluetooth mark. If the device reports its battery to Windows, the notice shows the level, in orange at 20% or below and red at 10% or below. Devices already connected when Zenith Dot starts are not announced.

Windows shows the volume and brightness flyouts. Zenith’s System panel provides audio output and per-app volume controls.

The accent colour follows the current cover. The tray menu has media controls, **Hide Zenith Dot**, and **Exit**. Hiding leaves playback running. Choose **Show Zenith Dot** from the tray to bring it back.

**Settings → Notifications** controls Do Not Disturb for Zenith alerts, including clipboard and timer bubbles. **Follow Windows** uses the current Windows notification mode, **On** always silences Zenith alerts, and **Off** (the default) leaves Zenith alerts enabled. The camera and microphone activity indicators remain visible when enabled.

Choose Jakarta, Makassar, or Jayapura under **Prayer timetable city** to show the five daily prayer times on an arc in the island’s Prayer tab. The arc shows the selected city’s local time, highlights the next prayer, and counts down to it, including tomorrow’s Subuh. Zenith calculates estimated times offline using the selected city’s time zone. Local official timetables can differ. Turn off **Prayer reminders** to keep the timetable visible without showing reminders.

### Connection status

The System panel shows the active connection and whether Windows reports internet access. Wi-Fi includes the network name when available. Zenith shows **Connection lost** or **Back online** after a change in internet availability. These alerts follow the island’s Do Not Disturb setting.

## Notifications from Windows apps

The MSIX build can show the app name and notification title for incoming Windows toasts. The user enables it with **Settings → Notifications → Connect Windows apps** and grants notification access in Windows. Zenith keeps the preview in memory while it is shown. Use **Test notification** to show a sample Clock alert on the island before connecting Windows apps.

To build the optional x64 MSIX using the Windows SDK:

```powershell
.\scripts\package-msix.ps1 -Executable .\out\Release\ZenithDot.exe -Output .\out\ZenithDot.msix -Publisher 'CN=Your signing publisher'
```

Sign the package with a trusted certificate whose subject matches the publisher, then install the signed MSIX with `Add-AppxPackage`. Exit the portable instance before opening the packaged app from Start. The portable executable supports Zenith reminders and Do Not Disturb.

For local Windows 11 testing, pass `-UnsignedTest` or download the MSIX test artifact from GitHub Actions. Install that package from PowerShell with `Add-AppxPackage -Path .\ZenithDot-test.msix -AllowUnsigned`. Windows may require an elevated PowerShell session.

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

In the extracted build package or `dist` directory, run `.\install.ps1` from PowerShell. The script installs for the current user and creates Start menu and Startup shortcuts on a first install. Upgrades preserve whether the Startup shortcut is enabled. Open **Zenith Dot** from Start to launch immediately. To uninstall, exit from the tray menu and run `.\uninstall.ps1` from the same package.

The Scratchpad note is stored in `%LOCALAPPDATA%\Zenith Dot\scratchpad.txt`.

## Choose between browser tabs

Chrome, Edge, and Brave publish a shared Windows media session for ordinary tabs. Use the optional **Zenith Dot Browser Tabs** extension in the build package for individual tab controls and to keep YouTube watch-page titles tied to the main player.

1. Install the current Zenith Dot build and run `.\enable-browser-tabs.ps1` from the package. To use a portable build, pass its executable via `-Executable` and run that same copy of Zenith Dot.
2. Open `chrome://extensions`, `edge://extensions`, or `brave://extensions`. Turn on Developer mode, choose **Load unpacked**, and select the package's `browser` folder. If the extension is already installed from another folder, replace it with the copy from this package.
3. Reload existing media tabs. Play two HTML media streams and hover Zenith Dot. Click a numbered browser logo to select that tab, then use play/pause or the seek bar where the media supports it.

The extension sends playback metadata and timing to the local native messaging host. It starts the companion when it has media to report. Clicking the cover activates the selected browser tab and its window. Without the extension, the cover can return to the browser window Windows reports as the media source. Previous and Next are available through Windows media sessions. Browser tab controls use the page's HTML audio or video element. If Windows reports different artwork metadata for a browser tab, Zenith Dot uses a music note until the track details match.

## Lyrics

Opening **Lyrics** sends the selected track's title, artist, duration, and album when available to [LRCLIB](https://lrclib.net/docs). When a matching track has synced lyrics and the player supplies a timeline, the side column shows the full lyrics within the height of Now Playing. Long lines wrap, and the active line uses a brighter text colour. Scroll over the lyrics to read ahead or back. Playback following resumes five seconds after the last scroll. If the player supports seeking, click any lyric line to jump to its timestamp.

With Lyrics enabled, the compact pill shows the current synced line beside the Lyrics icon. It widens to fit the text and wraps longer lines. New lines slide in as the old line fades, with resizing timed to make room for the text. The track title returns during a pause, before the first line, or at gaps marked in the lyrics. If Focus, Quick Timer, and media overlap, the pill prioritizes the active feature you selected most recently.

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
| `src/desktop.*` | Fullscreen detection, capture indicators, and audio device monitoring |
| `src/bluetooth.*` | Paired Bluetooth connection changes and battery levels |
| `src/scratchpad.*` | Native text editor, focus handoff, and local note storage |
| `src/browser_tabs.*` and `browser/` | Optional Chrome, Edge, and Brave tab detection and native messaging |
| `scripts/` | Per-user installation and removal |

## Verify on Windows 11

- Start Spotify or YouTube in a browser that publishes a Windows media session. Check track metadata and playback progress where the player provides them. Try Previous, Pause, and Next, then resume from the tray. While one player is playing, manually select a paused source. Verify that Zenith stays on the selected source and directs playback controls to it. Close that source and check that automatic selection resumes.
- Switch tracks in Spotify and return to a recent track. Its cached cover should appear again while Windows refreshes the thumbnail. Verify that a new track never displays the previous track's cover.
- With the browser extension enabled, pause a YouTube video and hover a recommendation. The pill and panel should keep the main video's title and timeline. Then navigate to another video in the same tab and check that both update together.
- Play tracks with differently coloured covers. The waveform and progress bar should fade to a colour from each cover, with a faint tint in the panel's lower-right corner. A greyscale cover or a player without artwork should fall back to light blue.
- Type in another app, then hover and click the island (including Previous, Pause, and Next). Focus and the caret must stay in that app. Right-click the island, close the menu, and confirm focus returns to the app.
- Hover the title pill to see its cover grow into the Now Playing panel. Move a little past the panel's edge and it should stay open, then close once the pointer is well away. Click outside it to close it right away.
- Switch between Now Playing, Timers, and System. With several players open, click the logo beside Lyrics and pick another source. Check that the panel stays the same height. Click the cover to return to the selected app or tab.
- Check a video longer than an hour. Both timeline timestamps should use hours. Close all media sources and confirm that **Not Playing** still shows the feature tabs in a shorter panel.
- Open Timers, adjust Focus and Break, then switch to Quick Timer while Focus is running and start a second countdown. Check each timer's pill, pause/resume, and completion bubble. Close and reopen the panel and check that the last timer view returns.
- Play audio in two apps and open System. Drag one app's slider and compare it with the Windows volume mixer, then click its icon to mute and unmute it. With more than three apps, scroll the list and check the range label.
- Turn on **Auto** in Timers, choose Break, set it to 1 minute, and start it. When it ends, check the **Break done** notice and that Focus starts with **Focus 2/4** in the pill. Restart Zenith Dot and confirm Auto is still on.
- Open System and switch the Windows default audio output. Check the displayed name and the Sound Settings link. Enable **Follow Windows**, turn on Windows Do Not Disturb, and verify the **Copied** bubble is suppressed. Switch Zenith Do Not Disturb to **Off** and copy again.
- Open Scratchpad, click the pin icon, and type a note. Switch to another app and confirm Scratchpad stays visible. Switch to Focus and System and check that the pin stays on. Unpin, close the panel, and hover again to check that System returns. Repeat with Lyrics open and manually scrolled. Reopen the editor, press Esc, and check that focus returns to the previous app. Restart Zenith Dot and confirm the note reloads.
- While music plays, choose **Hide Zenith Dot** from the tray. Confirm playback continues, then restore the island with **Show Zenith Dot**.
- With the browser extension enabled, play two tabs in the same browser and select each numbered logo. Verify that play/pause and seek affect the selected tab.
- Open Lyrics on a song with an LRCLIB match. Check that long lines wrap. Click a wrapped line and confirm playback jumps to its timestamp in a seekable player. Scroll to the first and last lines and check that playback following resumes five seconds after the last scroll. Switch tracks while a lookup is pending and check that the old track's lyrics disappear.
- Leave Lyrics enabled and move away from the panel. Check that the pill shows the active line, wraps a long line, and returns to the track title when paused. Trigger a Copy notice while the lyric pill is wide and verify that it splits off cleanly.
- Scroll over the compact pill and over the tab bar. Check the selected tab, the compact preview, and that scrolling inside Lyrics still moves the text.
- Pause playback and wait three seconds. The title should fade as the pill shrinks to the cover and pause icon. Hover to open the same track, then move away to return to the compact pill. With Lyrics open, check that the feature icons are centered under the full panel.
- Copy some text: a "Copied" bubble should stretch out of the pill, pinch off, show the first line, and merge back after about two seconds. Copy with the panel open: the notice should appear as a row at the top of the panel. Copy a password from a password manager: no notice.
- Enable **Add copied text to Drop Zone** and turn off **Show clipboard previews**. Copy two lines of text and verify that the bubble says **Copied** and that dragging the new item out of Drop Zone transfers both lines.
- Capture a screenshot with Windows+Shift+S and open **Drop Zone**. Check that one thumbnail appears, click it to open the image, and close the preview to return to the list. Drag the row into Explorer. Add another file or text and check that they share the list. Clear the list and check that the dashed area returns to its empty layout.
- Unplug and reconnect the charger: "On Battery" and then a green "Charging" bubble, each with the battery level.
- Open System and check the battery percentage and AC status. Unplug and reconnect the charger. On a desktop without a battery, confirm the panel reports that state.
- Enter and leave fullscreen in a video player on the primary display. The island should fade out, close its panel, and return after you exit fullscreen. Notices raised during fullscreen should expire without appearing afterwards.
- Start microphone and camera capture in an app. Check that both icons appear. With capture still open, mute the microphone in Windows and verify that the orange icon disappears and the green one remains. Unmute and verify that the orange icon returns. With two active microphones, muting one should leave the icon visible.
- Connect paired Bluetooth earbuds. After about two seconds a notice should show the device name with its battery level, or **Connected** if it reports none. Disconnect them and check the grey **Disconnected** notice. Restart Zenith Dot with the earbuds connected and confirm nothing is announced.
- Change volume and brightness with the keyboard and check that Windows shows the flyouts.
- Install, sign out, sign back in, and confirm there is one Zenith Dot process. Launch it again from Start and check that the process count stays at one.
- Compare private working set after a minute at idle and during playback. For an Electron baseline, include its renderer and helper processes.
