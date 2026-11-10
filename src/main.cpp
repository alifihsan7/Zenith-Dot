#include <initguid.h>

#include "browser_tabs.hpp"
#include "desktop.hpp"
#include "drop_zone.hpp"
#include "island.hpp"
#include <hidusage.h>
#include <ole2.h>
#include <set>
#include <shellapi.h>
#include <windowsx.h>

namespace zenith {
namespace {

using namespace std::chrono_literals;

constexpr wchar_t appName[] = L"Zenith Dot";
constexpr UINT trayMessage = WM_APP + 2;
constexpr UINT redrawMessage = WM_APP + 3;
constexpr UINT_PTR powerTimer = 1;
constexpr UINT_PTR frameTimer = 2;
constexpr UINT_PTR fullscreenTimer = 3;
constexpr UINT_PTR clipboardTimer = 5;
static_assert(clipboardTimer != MediaMonitor::artworkTimer);
constexpr UINT clipboardRetryInterval = 40;
constexpr unsigned clipboardRetryLimit = 25;
constexpr unsigned microphoneColor = 0xff9f0a;
constexpr unsigned cameraColor = 0x30d158;

enum Menu { previousTrack = 1, togglePlayback, nextTrack, toggleVisibility, openDropZone, exitApp };
constexpr MediaCommand controls[] = {MediaCommand::previous, MediaCommand::toggle, MediaCommand::next};

struct LockKey {
    USHORT virtualKey;
    IslandAlert::Kind kind;
    wchar_t const* label;
    bool enabled = false;
    std::set<HANDLE> held{};
};

class App {
public:
    int run(HINSTANCE instance);

private:
    static LRESULT CALLBACK dispatch(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT handle(UINT message, WPARAM wparam, LPARAM lparam);
    void frame();
    void updateMedia();
    void control(MediaCommand command);
    void run(IslandCommand const& command);
    void refreshPower();
    void refreshMotion();
    bool refreshFullscreen();
    void applyVisibility();
    bool suppressed() const { return fullscreen_ || hidden_; }
    void refreshCapture(bool announce = true);
    void showLevel(IslandAlert::Kind kind, unsigned percent, bool muted = false);
    void addTray();
    void showMenu(POINT point);
    void showAudioPicker();
    void queueClipboard(bool listenerEvent = false);
    void refreshClipboard();
    void updateLockKey(RAWKEYBOARD const& keyboard, HANDLE device);
    std::optional<IslandAlert> clipboardAlert(bool& busy);

    HWND window_ = nullptr;
    HPOWERNOTIFY powerNotification_ = nullptr;
    UINT taskbarCreated_ = 0;
    UINT frameInterval_ = 0;
    NOTIFYICONDATAW tray_{};
    std::shared_ptr<MediaMonitor> monitor_;
    std::shared_ptr<Lyrics> lyrics_;
    MediaState media_;
    MediaState nativeMedia_;
    BrowserTabs browserTabs_;
    PowerState power_;
    DesktopMonitor desktop_;
    CaptureActivity capture_;
    std::optional<WPARAM> lastVolume_;
    SystemMonitor system_;
    Scratchpad scratchpad_;
    DropZone dropZone_;
    std::chrono::steady_clock::time_point systemSampled_{};
    Island island_;
    bool tracking_ = false;
    bool fullscreen_ = false;
    bool hidden_ = false;
    bool clipboardListening_ = false;
    DWORD clipboardSequence_ = 0;
    std::optional<unsigned> clipboardAttempts_;
    unsigned captureTick_ = 0;
    unsigned lowWarned_ = 0;   // the low-battery threshold (20 or 10) already announced this discharge
    LockKey lockKeys_[2]{
        {VK_CAPITAL, IslandAlert::Kind::capsLock, L"Caps Lock"},
        {VK_NUMLOCK, IslandAlert::Kind::numLock, L"Num Lock"},
    };
};

int App::run(HINSTANCE instance) {
    WNDCLASSW type{};
    type.lpfnWndProc = dispatch;
    type.hInstance = instance;
    type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    type.lpszClassName = overlayWindowClass;
    winrt::check_bool(RegisterClassW(&type));
    // WS_EX_NOACTIVATE: clicks and hover reach the island, but the app the user is working in keeps focus.
    window_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        overlayWindowClass, appName, WS_POPUP, 0, 0, 16, 16, nullptr, nullptr, instance, this);
    winrt::check_bool(window_ != nullptr);
    browserTabs_.setWindow(window_);

    taskbarCreated_ = RegisterWindowMessageW(L"TaskbarCreated");
    refreshMotion();
    power_ = readPower();
    island_.setPower(power_);
    island_.setSystem(system_.sample());
    powerNotification_ = RegisterPowerSettingNotification(window_, &GUID_ACDC_POWER_SOURCE,
                                                           DEVICE_NOTIFY_WINDOW_HANDLE);
    SetTimer(window_, powerTimer, 10'000, nullptr);
    SetTimer(window_, fullscreenTimer, 750, nullptr);
    clipboardSequence_ = GetClipboardSequenceNumber();
    clipboardListening_ = AddClipboardFormatListener(window_) != FALSE;
    for (auto& key : lockKeys_) key.enabled = (GetKeyState(key.virtualKey) & 1) != 0;
    const RAWINPUTDEVICE keyboard{HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_KEYBOARD, RIDEV_INPUTSINK, window_};
    if (!RegisterRawInputDevices(&keyboard, 1, sizeof(keyboard))) {
        OutputDebugStringW(L"Zenith Dot could not register lock key input.\n");
    }
    dropZone_.attach(window_, [this](bool hovering) {
        island_.setDropHover(hovering);
        frame();
    }, [this](std::vector<DropPreview> items) {
        island_.setDropItems(std::move(items));
        frame();
    });
    desktop_.start(window_);
    island_.setAudioOutput(desktop_.audioOutput());
    island_.setNotificationsQuiet(desktop_.notificationsQuiet());
    lyrics_ = std::make_shared<Lyrics>([this] { frame(); });
    island_.setLyrics(lyrics_->view());
    scratchpad_.load();
    island_.setScratchpad(scratchpad_.view());
    refreshFullscreen();
    refreshCapture(false);
    addTray();
    frame();
    monitor_ = std::make_shared<MediaMonitor>(window_, [this](MediaState const& state) {
        nativeMedia_ = state;
        updateMedia();
    });
    monitor_->start();

    MSG message{};
    BOOL result;
    while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (result < 0) winrt::throw_last_error();
    return static_cast<int>(message.wParam);
}

void App::frame() {
    island_.tick();
    if (scratchpad_.window() && !island_.scratchEditorRect()) {
        scratchpad_.close();
        island_.setScratchEditing(false);
    }
    if (lyrics_) lyrics_->update(media_, island_.wantsLyrics(), island_.wantsLyricPill());
    const auto now = std::chrono::steady_clock::now();
    if (island_.wantsSystemStats() && now - systemSampled_ >= 1s) {
        island_.setSystem(system_.sample());
        systemSampled_ = now;
    }
    const bool shown = IsWindowVisible(window_) != FALSE;
    const bool visible = island_.visible() && (shown || !suppressed());
    if (visible) island_.draw(window_);
    if (scratchpad_.window()) {
        if (const auto bounds = island_.scratchEditorRect()) scratchpad_.place(*bounds);
    }
    if (visible != shown) ShowWindow(window_, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
    const UINT interval = island_.frameInterval();
    if (interval == frameInterval_) return;
    if (frameInterval_ != 0) KillTimer(window_, frameTimer);
    frameInterval_ = interval;
    if (frameInterval_ != 0) SetTimer(window_, frameTimer, frameInterval_, nullptr);
}

void App::updateMedia() {
    media_ = browserTabs_.combine(nativeMedia_);
    island_.setMedia(media_);
    frame();
}

void App::control(MediaCommand command) {
    if (!media_.sourceId.empty()) browserTabs_.control(media_.sourceId, command);
    else if (monitor_) monitor_->control(command);
}

void App::run(IslandCommand const& command) {
    switch (command.kind) {
    case IslandCommand::Kind::previous: control(MediaCommand::previous); break;
    case IslandCommand::Kind::toggle: control(MediaCommand::toggle); break;
    case IslandCommand::Kind::next: control(MediaCommand::next); break;
    case IslandCommand::Kind::seek:
        if (media_.sourceId.empty()) {
            if (monitor_) monitor_->seek(command.seconds);
        }
        else browserTabs_.seek(media_.sourceId, command.seconds);
        break;
    case IslandCommand::Kind::selectSource:
        if (browserTabs_.select(command.appId)) updateMedia();
        else if (monitor_) monitor_->select(command.appId);
        break;
    case IslandCommand::Kind::retryLyrics: if (lyrics_) lyrics_->retry(); break;
    case IslandCommand::Kind::editScratchpad:
        if (const auto bounds = island_.scratchEditorRect()) {
            island_.setScratchEditing(scratchpad_.open(window_, *bounds));
        }
        break;
    case IslandCommand::Kind::audioPicker: showAudioPicker(); break;
    case IslandCommand::Kind::clearDrop: dropZone_.clear(); break;
    case IslandCommand::Kind::removeDrop: dropZone_.remove(command.index); break;
    case IslandCommand::Kind::notificationSettings:
        ShellExecuteW(window_, L"open", L"ms-settings:notifications", nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case IslandCommand::Kind::none: break;
    }
}

void App::refreshPower() {
    const auto state = readPower();
    if (power_ == state) return;
    const auto previous = power_;
    power_ = state;
    island_.setPower(power_);
    const auto level = power_.percent ? std::to_wstring(*power_.percent) + L"%" : std::wstring{};
    if (previous.pluggedIn && power_.pluggedIn && previous.pluggedIn != power_.pluggedIn) {
        if (*power_.pluggedIn) {
            island_.notify({IslandAlert::Kind::charging, L"Charging", level, 0x30d158u, 2});
        } else {
            island_.notify({IslandAlert::Kind::battery, L"On Battery", level, std::nullopt, 2});
        }
    }
    // Low battery: once at 20% and once more at 10% per discharge.
    if (power_.pluggedIn != false || !power_.percent || *power_.percent > 20) {
        lowWarned_ = 0;
    } else {
        const unsigned threshold = *power_.percent <= 10 ? 10 : 20;
        if (lowWarned_ == 0 || threshold < lowWarned_) {
            lowWarned_ = threshold;
            island_.notify({IslandAlert::Kind::lowBattery, L"Low Battery", level,
                            threshold == 10 ? 0xff453au : 0xff9f0au, 3});
        }
    }
    frame();
}

void App::refreshMotion() {
    BOOL animations = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0);
    island_.setAnimate(animations != FALSE);
}

bool App::refreshFullscreen() {
    const auto foreground = GetForegroundWindow();
    if (foreground && (foreground == window_ || foreground == scratchpad_.window())) return false;
    const bool fullscreen = fullscreenOnPrimary(window_);
    if (fullscreen == fullscreen_) return false;
    fullscreen_ = fullscreen;
    applyVisibility();
    return true;
}

void App::applyVisibility() {
    const bool hide = suppressed();
    island_.setSuppressed(hide);
    const auto style = GetWindowLongPtrW(window_, GWL_EXSTYLE);
    SetWindowLongPtrW(window_, GWL_EXSTYLE, hide ? style | WS_EX_TRANSPARENT : style & ~WS_EX_TRANSPARENT);
    if (hide && GetCapture() == window_) ReleaseCapture();
}

void App::refreshCapture(bool announce) {
    auto activity = desktop_.captureActivity();
    if (activity == capture_) return;
    island_.setCaptureColors(activity.microphone.empty() ? std::nullopt : std::optional(microphoneColor),
                             activity.camera.empty() ? std::nullopt : std::optional(cameraColor));
    if (announce) {
        if (capture_.microphone != activity.microphone && !activity.microphone.empty()) {
            island_.notify({IslandAlert::Kind::microphone, L"Microphone", activity.microphone, microphoneColor, 2});
        }
        if (capture_.camera != activity.camera && !activity.camera.empty()) {
            island_.notify({IslandAlert::Kind::camera, L"Camera", activity.camera, cameraColor, 2});
        }
    }
    capture_ = std::move(activity);
    if (announce) frame();
}

void App::showLevel(IslandAlert::Kind kind, unsigned percent, bool muted) {
    if (suppressed()) return;
    island_.notify({kind, L"", muted ? L"Muted" : std::to_wstring(percent) + L"%", std::nullopt,
                    1, 1800ms, muted ? 0u : percent});
    frame();
}

void App::addTray() {
    tray_.cbSize = sizeof(tray_);
    tray_.hWnd = window_;
    tray_.uID = 1;
    tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    tray_.uCallbackMessage = trayMessage;
    tray_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(tray_.szTip, appName);
    if (Shell_NotifyIconW(NIM_ADD, &tray_)) {
        tray_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &tray_);
    }
}

void App::showMenu(POINT point) {
    const auto menu = CreatePopupMenu();
    if (!menu) return;
    const auto skip = media_.hasSession && media_.canSkip ? MF_ENABLED : MF_GRAYED;
    AppendMenuW(menu, MF_STRING | skip, previousTrack, L"&Previous track");
    AppendMenuW(menu, MF_STRING, togglePlayback, media_.playing ? L"P&ause" : L"&Play");
    AppendMenuW(menu, MF_STRING | skip, nextTrack, L"&Next track");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, toggleVisibility, hidden_ ? L"&Show Zenith Dot" : L"&Hide Zenith Dot");
    AppendMenuW(menu, MF_STRING | (suppressed() ? MF_GRAYED : MF_ENABLED), openDropZone, L"&Drop Zone");
    AppendMenuW(menu, MF_STRING, exitApp, L"E&xit Zenith Dot");
    // TrackPopupMenu needs the owner in the foreground to dismiss correctly. Hand focus back afterwards.
    const auto previous = GetForegroundWindow();
    SetForegroundWindow(window_);
    const auto selected = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                                         point.x, point.y, 0, window_, nullptr);
    DestroyMenu(menu);
    PostMessageW(window_, WM_NULL, 0, 0);
    if (previous && previous != window_ && IsWindow(previous)) SetForegroundWindow(previous);
    if (selected == exitApp) {
        DestroyWindow(window_);
    } else if (selected == toggleVisibility) {
        hidden_ = !hidden_;
        applyVisibility();
        frame();
    } else if (selected == openDropZone) {
        island_.showDropZone();
        frame();
    } else if (selected >= previousTrack && selected <= nextTrack && monitor_) {
        control(controls[selected - previousTrack]);
    }
}

void App::showAudioPicker() {
    const auto outputs = desktop_.audioOutputs();
    const auto menu = CreatePopupMenu();
    if (!menu) return;
    const auto settings = static_cast<UINT>(outputs.size()) + 1;
    for (size_t i = 0; i < outputs.size(); ++i) {
        auto label = outputs[i].name;
        for (size_t pos = 0; (pos = label.find(L'&', pos)) != std::wstring::npos; pos += 2) {
            label.insert(pos, 1, L'&');
        }
        AppendMenuW(menu, MF_STRING | (outputs[i].active ? MF_CHECKED : MF_UNCHECKED), i + 1, label.c_str());
    }
    if (outputs.empty()) AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"No outputs available");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, settings, L"Sound settings…");
    POINT point{};
    GetCursorPos(&point);
    const auto previous = GetForegroundWindow();
    island_.setPopupOpen(true);
    SetForegroundWindow(window_);
    const auto selected = static_cast<UINT>(
        TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, point.x, point.y, 0, window_, nullptr));
    island_.setPopupOpen(false);
    DestroyMenu(menu);
    PostMessageW(window_, WM_NULL, 0, 0);
    if (previous && previous != window_ && IsWindow(previous)) SetForegroundWindow(previous);
    if (selected == settings) {
        ShellExecuteW(window_, L"open", L"ms-settings:sound", nullptr, nullptr, SW_SHOWNORMAL);
    } else if (selected > 0 && selected <= outputs.size() && !outputs[selected - 1].active) {
        if (!desktop_.selectAudioOutput(outputs[selected - 1].id)) {
            MessageBoxW(window_, L"Windows couldn't switch to that audio output.", appName, MB_OK | MB_ICONWARNING);
        }
        desktop_.refreshAudioDevice();
        island_.setAudioOutput(desktop_.audioOutput());
    }
}

// "Copied" plus a short preview. Nothing is kept. Content that password managers mark as private
// (excluded from monitoring or from clipboard history) is not announced at all.
std::optional<IslandAlert> App::clipboardAlert(bool& busy) {
    static const UINT excluded = RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing");
    static const UINT history = RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");
    busy = !OpenClipboard(window_);
    if (busy) return std::nullopt;
    struct Close {
        DWORD& sequence;
        ~Close() {
            sequence = GetClipboardSequenceNumber();
            CloseClipboard();
        }
    } close{clipboardSequence_};
    if (IsClipboardFormatAvailable(excluded)) return std::nullopt;
    IslandAlert alert{IslandAlert::Kind::copy, L"Copied"};
    bool hidden = false;
    if (const auto flag = IsClipboardFormatAvailable(history) ? GetClipboardData(history) : nullptr) {
        if (GlobalSize(flag) >= sizeof(DWORD)) {
            if (const auto value = static_cast<DWORD const*>(GlobalLock(flag))) {
                hidden = *value == 0;
                GlobalUnlock(flag);
            }
        }
    }
    if (hidden) return std::nullopt;
    if (const auto data = GetClipboardData(CF_UNICODETEXT)) {
        if (const auto text = static_cast<wchar_t const*>(GlobalLock(data))) {
            // First non-empty line, trimmed, at most 60 characters.
            std::wstring_view all(text, wcsnlen(text, GlobalSize(data) / sizeof(wchar_t)));
            while (!all.empty()) {
                const auto end = all.find_first_of(L"\r\n");
                auto line = all.substr(0, end);
                const auto first = line.find_first_not_of(L" \t");
                if (first != std::wstring_view::npos) {
                    line = line.substr(first, line.find_last_not_of(L" \t") - first + 1);
                    alert.value = std::wstring(line.substr(0, 60));
                    break;
                }
                if (end == std::wstring_view::npos) break;
                all.remove_prefix(end + 1);
            }
            GlobalUnlock(data);
        }
    } else if (const auto drop = static_cast<HDROP>(GetClipboardData(CF_HDROP))) {
        const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        wchar_t name[MAX_PATH]{};
        if (count == 1 && DragQueryFileW(drop, 0, name, MAX_PATH)) {
            const auto slash = std::wstring_view(name).find_last_of(L"\\/");
            alert.value = slash == std::wstring_view::npos ? name : name + slash + 1;
        } else if (count > 1) {
            alert.value = std::to_wstring(count) + L" files";
        }
    } else if (IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(CF_BITMAP)) {
        alert.value = L"Image";
    }
    return alert;
}

void App::queueClipboard(bool listenerEvent) {
    if (clipboardAttempts_) return;
    // A listener event can arrive before a delayed format advances the sequence number.
    if (!listenerEvent) {
        const auto sequence = GetClipboardSequenceNumber();
        if (!sequence || sequence == clipboardSequence_) return;
        clipboardSequence_ = sequence;
    }
    clipboardAttempts_ = 0;
    if (!SetTimer(window_, clipboardTimer, clipboardRetryInterval, nullptr)) refreshClipboard();
}

void App::refreshClipboard() {
    if (!clipboardAttempts_) return;
    island_.setNotificationsQuiet(desktop_.notificationsQuiet());
    if (suppressed() || island_.notificationsQuiet()) {
        clipboardSequence_ = GetClipboardSequenceNumber();
        KillTimer(window_, clipboardTimer);
        clipboardAttempts_.reset();
        return;
    }
    bool busy = false;
    auto alert = clipboardAlert(busy);
    if (busy && ++*clipboardAttempts_ < clipboardRetryLimit) {
        if (SetTimer(window_, clipboardTimer, clipboardRetryInterval, nullptr)) return;
    }
    KillTimer(window_, clipboardTimer);
    clipboardAttempts_.reset();
    if (alert) {
        island_.notify(std::move(*alert));
        frame();
    }
}

void App::updateLockKey(RAWKEYBOARD const& keyboard, HANDLE device) {
    for (auto& key : lockKeys_) {
        if (keyboard.VKey != key.virtualKey) continue;
        if (keyboard.Flags & RI_KEY_BREAK) {
            key.held.erase(device);
        } else if (key.held.insert(device).second) {
            key.enabled = !key.enabled;
            island_.notify({key.kind, key.label, key.enabled ? L"On" : L"Off", std::nullopt, 1, 1100ms});
            frame();
        }
        return;
    }
}

LRESULT CALLBACK App::dispatch(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCCREATE) {
        const auto creation = reinterpret_cast<CREATESTRUCTW*>(lparam);
        auto* app = static_cast<App*>(creation->lpCreateParams);
        app->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    try {
        return app ? app->handle(message, wparam, lparam) : DefWindowProcW(window, message, wparam, lparam);
    } catch (winrt::hresult_error const& error) {
        OutputDebugStringW(error.message().c_str());
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

LRESULT App::handle(UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == taskbarCreated_ && taskbarCreated_ != 0) {
        addTray();
        return 0;
    }

    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        BeginPaint(window_, &paint);
        EndPaint(window_, &paint);
        frame();
        return 0;
    }
    case WM_SETTINGCHANGE:
        refreshMotion();
        island_.setNotificationsQuiet(desktop_.notificationsQuiet());
        [[fallthrough]];
    case WM_DISPLAYCHANGE:
    case WM_DPICHANGED:
        PostMessageW(window_, redrawMessage, 0, 0);
        return 0;
    case redrawMessage:
        refreshFullscreen();
        frame();
        return 0;
    case WM_POWERBROADCAST:
        refreshPower();
        return TRUE;
    case WM_TIMER:
        if (wparam == MediaMonitor::artworkTimer && monitor_) monitor_->retryArtwork();
        if (wparam == powerTimer) {
            if (!clipboardListening_) clipboardListening_ = AddClipboardFormatListener(window_) != FALSE;
            const bool changed = island_.setNotificationsQuiet(desktop_.notificationsQuiet());
            refreshPower();
            if (changed) frame();
        }
        if (wparam == frameTimer) frame();
        if (wparam == clipboardTimer) refreshClipboard();
        if (wparam == fullscreenTimer) {
            const bool changed = refreshFullscreen();
            queueClipboard();
            if (++captureTick_ == 4) {
                captureTick_ = 0;
                refreshCapture();
            }
            if (browserTabs_.prune()) updateMedia();
            else if (changed) frame();
        }
        return 0;
    case WM_COPYDATA:
        if (const auto packet = reinterpret_cast<COPYDATASTRUCT const*>(lparam);
            packet && browserTabs_.receive(reinterpret_cast<HWND>(wparam), *packet)) {
            updateMedia();
            return TRUE;
        }
        return FALSE;
    case DesktopMonitor::foregroundChanged:
        if (refreshFullscreen()) frame();
        return 0;
    case DesktopMonitor::captureChanged:
        refreshCapture();
        return 0;
    case DesktopMonitor::audioDeviceChanged:
        lastVolume_.reset();
        desktop_.refreshAudioDevice();
        island_.setAudioOutput(desktop_.audioOutput());
        frame();
        return 0;
    case DesktopMonitor::notificationModeChanged:
        if (island_.setNotificationsQuiet(desktop_.notificationsQuiet())) frame();
        return 0;
    case DesktopMonitor::volumeChanged:
        if (desktop_.isCurrentVolume(lparam) && wparam != lastVolume_) {
            lastVolume_ = wparam;
            showLevel(IslandAlert::Kind::volume, static_cast<unsigned>(wparam & 0xff), (wparam & 0x100) != 0);
        }
        return 0;
    case DesktopMonitor::brightnessChanged:
        showLevel(IslandAlert::Kind::brightness, static_cast<unsigned>(wparam));
        return 0;
    case MediaMonitor::notification:
        if (monitor_) monitor_->refresh(wparam);
        return 0;
    case WM_CLIPBOARDUPDATE:
        queueClipboard(true);
        return 0;
    case WM_INPUT: {
        RAWINPUT input{};
        UINT bytes = sizeof(input);
        const UINT read = GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT,
                                          &input, &bytes, sizeof(RAWINPUTHEADER));
        if (read != static_cast<UINT>(-1) && input.header.dwType == RIM_TYPEKEYBOARD) {
            updateLockKey(input.data.keyboard, input.header.hDevice);
        }
        return DefWindowProcW(window_, message, wparam, lparam);
    }
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_SETCURSOR:
        if (LOWORD(lparam) == HTCLIENT) {
            SetCursor(LoadCursorW(nullptr, island_.overControl() ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        break;
    case WM_MOUSEMOVE:
        if (suppressed()) return 0;
        if (!tracking_) {
            TRACKMOUSEEVENT tracking{sizeof(TRACKMOUSEEVENT), TME_LEAVE, window_, 0};
            tracking_ = TrackMouseEvent(&tracking) != FALSE;
        }
        if (island_.pointerMove({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)})) {
            SetCursor(LoadCursorW(nullptr, island_.overControl() ? IDC_HAND : IDC_ARROW));
            frame();
        }
        return 0;
    case WM_MOUSELEAVE:
        tracking_ = false;
        if (island_.pointerLeave()) frame();
        return 0;
    case WM_MOUSEWHEEL: {
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ScreenToClient(window_, &point);
        if (island_.scroll(point, GET_WHEEL_DELTA_WPARAM(wparam))) frame();
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (const auto item = island_.dropItemAt(point)) {
            ClientToScreen(window_, &point);
            if (DragDetect(window_, point) && IsWindow(window_)) dropZone_.drag(*item);
        } else if (island_.pointerDown(point)) {
            SetCapture(window_);
        }
        frame();
        return 0;
    }
    case WM_LBUTTONUP: {
        const auto command = island_.pointerUp({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        if (GetCapture() == window_) ReleaseCapture();
        run(command);
        frame();
        return 0;
    }
    case WM_CAPTURECHANGED:
        island_.pointerCancel();
        frame();
        return 0;
    case Scratchpad::dismissed:
        if (scratchpad_.window() == reinterpret_cast<HWND>(wparam)) {
            const bool explicitClose = lparam == Scratchpad::explicitDismiss;
            scratchpad_.close(explicitClose);
            island_.setScratchEditing(false);
            if (explicitClose || !island_.panelPinned()) island_.closePanel();
            frame();
        }
        return 0;
    case WM_CONTEXTMENU: {
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (point.x == -1 && point.y == -1) ClientToScreen(window_, &point);
        showMenu(point);
        return 0;
    }
    case trayMessage:
        if (LOWORD(lparam) == WM_CONTEXTMENU || LOWORD(lparam) == NIN_KEYSELECT ||
            LOWORD(lparam) == NIN_SELECT) {
            showMenu({GET_X_LPARAM(wparam), GET_Y_LPARAM(wparam)});
        }
        return 0;
    case WM_DESTROY:
        dropZone_.detach();
        scratchpad_.close();
        KillTimer(window_, powerTimer);
        KillTimer(window_, frameTimer);
        KillTimer(window_, fullscreenTimer);
        KillTimer(window_, clipboardTimer);
        desktop_.stop();
        if (lyrics_) lyrics_->stop();
        RemoveClipboardFormatListener(window_);
        if (monitor_) monitor_->stop();
        if (powerNotification_) UnregisterPowerSettingNotification(powerNotification_);
        Shell_NotifyIconW(NIM_DELETE, &tray_);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window_, message, wparam, lparam);
}

}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    if (zenith::browserHostRequested()) return zenith::runBrowserHost(instance);
    const auto singleInstance = CreateMutexW(nullptr, FALSE, L"Local\\ZenithDot.Overlay.0A54ED53");
    const auto error = GetLastError();
    if (!singleInstance) return 1;
    if (error == ERROR_ALREADY_EXISTS) {
        CloseHandle(singleInstance);
        return 0;
    }
    int result = 1;
    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        struct Ole {
            HRESULT status = OleInitialize(nullptr);
            ~Ole() { if (SUCCEEDED(status)) OleUninitialize(); }
        } ole;
        result = zenith::App{}.run(instance);
    } catch (winrt::hresult_error const& failure) {
        MessageBoxW(nullptr, failure.message().c_str(), L"Zenith Dot could not start", MB_OK | MB_ICONERROR);
    }
    CloseHandle(singleInstance);
    return result;
}
