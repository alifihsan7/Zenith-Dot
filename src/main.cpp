#include <initguid.h>

#include "island.hpp"
#include <shellapi.h>
#include <windowsx.h>

namespace zenith {
namespace {

using namespace std::chrono_literals;

constexpr wchar_t appName[] = L"Zenith Dot";
constexpr wchar_t windowClass[] = L"ZenithDot.Overlay";
constexpr UINT trayMessage = WM_APP + 2;
constexpr UINT redrawMessage = WM_APP + 3;
constexpr UINT_PTR powerTimer = 1;
constexpr UINT_PTR frameTimer = 2;

enum Menu { previousTrack = 1, togglePlayback, nextTrack, exitApp };
constexpr MediaCommand controls[] = {MediaCommand::previous, MediaCommand::toggle, MediaCommand::next};

class App {
public:
    int run(HINSTANCE instance);

private:
    static LRESULT CALLBACK dispatch(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT handle(UINT message, WPARAM wparam, LPARAM lparam);
    void frame();
    void run(IslandCommand const& command);
    void refreshPower();
    void refreshMotion();
    void addTray();
    void showMenu(POINT point);
    std::optional<IslandAlert> clipboardAlert();

    HWND window_ = nullptr;
    HPOWERNOTIFY powerNotification_ = nullptr;
    UINT taskbarCreated_ = 0;
    UINT frameInterval_ = 0;
    NOTIFYICONDATAW tray_{};
    std::shared_ptr<MediaMonitor> monitor_;
    MediaState media_;
    PowerState power_;
    SystemMonitor system_;
    std::chrono::steady_clock::time_point systemSampled_{};
    Island island_;
    bool tracking_ = false;
    unsigned lowWarned_ = 0;   // the low-battery threshold (20 or 10) already announced this discharge
};

int App::run(HINSTANCE instance) {
    WNDCLASSW type{};
    type.lpfnWndProc = dispatch;
    type.hInstance = instance;
    type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    type.lpszClassName = windowClass;
    winrt::check_bool(RegisterClassW(&type));
    // WS_EX_NOACTIVATE: clicks and hover reach the island, but the app the user is working in keeps focus.
    window_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        windowClass, appName, WS_POPUP, 0, 0, 16, 16, nullptr, nullptr, instance, this);
    winrt::check_bool(window_ != nullptr);

    taskbarCreated_ = RegisterWindowMessageW(L"TaskbarCreated");
    refreshMotion();
    power_ = readPower();
    island_.setPower(power_);
    island_.setSystem(system_.sample());
    powerNotification_ = RegisterPowerSettingNotification(window_, &GUID_ACDC_POWER_SOURCE,
                                                           DEVICE_NOTIFY_WINDOW_HANDLE);
    SetTimer(window_, powerTimer, 10'000, nullptr);
    AddClipboardFormatListener(window_);
    addTray();
    frame();
    ShowWindow(window_, SW_SHOWNOACTIVATE);
    monitor_ = std::make_shared<MediaMonitor>(window_, [this](MediaState const& state) {
        media_ = state;
        island_.setMedia(state);
        frame();
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
    const auto now = std::chrono::steady_clock::now();
    if (island_.wantsSystemStats() && now - systemSampled_ >= 1s) {
        island_.setSystem(system_.sample());
        systemSampled_ = now;
    }
    island_.draw(window_);
    const UINT interval = island_.frameInterval();
    if (interval == frameInterval_) return;
    if (frameInterval_ != 0) KillTimer(window_, frameTimer);
    frameInterval_ = interval;
    if (frameInterval_ != 0) SetTimer(window_, frameTimer, frameInterval_, nullptr);
}

void App::run(IslandCommand const& command) {
    if (!monitor_) return;
    switch (command.kind) {
    case IslandCommand::Kind::previous: monitor_->control(MediaCommand::previous); break;
    case IslandCommand::Kind::toggle: monitor_->control(MediaCommand::toggle); break;
    case IslandCommand::Kind::next: monitor_->control(MediaCommand::next); break;
    case IslandCommand::Kind::seek: monitor_->seek(command.seconds); break;
    case IslandCommand::Kind::selectSource: monitor_->select(command.appId); break;
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
    AppendMenuW(menu, MF_STRING | (media_.hasSession ? MF_ENABLED : MF_GRAYED), previousTrack, L"&Previous track");
    AppendMenuW(menu, MF_STRING, togglePlayback, media_.playing ? L"P&ause" : L"&Play");
    AppendMenuW(menu, MF_STRING | (media_.hasSession ? MF_ENABLED : MF_GRAYED), nextTrack, L"&Next track");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
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
    } else if (selected >= previousTrack && selected <= nextTrack && monitor_) {
        monitor_->control(controls[selected - previousTrack]);
    }
}

// "Copied" plus a short preview. Nothing is kept. Content that password managers mark as private
// (excluded from monitoring or from clipboard history) is not announced at all.
std::optional<IslandAlert> App::clipboardAlert() {
    static const UINT excluded = RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing");
    static const UINT history = RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");
    if (IsClipboardFormatAvailable(excluded)) return std::nullopt;
    IslandAlert alert{IslandAlert::Kind::copy, L"Copied"};
    if (!OpenClipboard(window_)) return alert;
    bool hidden = false;
    if (const auto flag = IsClipboardFormatAvailable(history) ? GetClipboardData(history) : nullptr) {
        if (const auto value = static_cast<DWORD const*>(GlobalLock(flag))) {
            hidden = *value == 0;
            GlobalUnlock(flag);
        }
    }
    if (hidden) {
        CloseClipboard();
        return std::nullopt;
    }
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
    CloseClipboard();
    return alert;
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
        [[fallthrough]];
    case WM_DISPLAYCHANGE:
    case WM_DPICHANGED:
        PostMessageW(window_, redrawMessage, 0, 0);
        return 0;
    case redrawMessage:
        frame();
        return 0;
    case WM_POWERBROADCAST:
        refreshPower();
        return TRUE;
    case WM_TIMER:
        if (wparam == powerTimer) refreshPower();
        if (wparam == frameTimer) frame();
        return 0;
    case MediaMonitor::notification:
        if (monitor_) monitor_->refresh(wparam);
        return 0;
    case WM_CLIPBOARDUPDATE:
        if (auto alert = clipboardAlert()) {
            island_.notify(std::move(*alert));
            frame();
        }
        return 0;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_SETCURSOR:
        if (LOWORD(lparam) == HTCLIENT) {
            SetCursor(LoadCursorW(nullptr, island_.overControl() ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        break;
    case WM_MOUSEMOVE:
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
    case WM_LBUTTONDOWN:
        if (island_.pointerDown({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)})) SetCapture(window_);
        frame();
        return 0;
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
        KillTimer(window_, powerTimer);
        KillTimer(window_, frameTimer);
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
        result = zenith::App{}.run(instance);
    } catch (winrt::hresult_error const& failure) {
        MessageBoxW(nullptr, failure.message().c_str(), L"Zenith Dot could not start", MB_OK | MB_ICONERROR);
    }
    CloseHandle(singleInstance);
    return result;
}
