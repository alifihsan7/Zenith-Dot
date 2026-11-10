#include <initguid.h>

#include "island.hpp"
#include <shellapi.h>
#include <windowsx.h>

namespace zenith {
namespace {

constexpr wchar_t appName[] = L"Zenith Dot";
constexpr wchar_t windowClass[] = L"ZenithDot.Overlay";
constexpr UINT trayMessage = WM_APP + 2;
constexpr UINT redrawMessage = WM_APP + 3;
constexpr UINT_PTR powerTimer = 1;
constexpr UINT_PTR renderTimer = 2;

enum Menu { showBattery = 1, previousTrack, togglePlayback, nextTrack, exitApp };
constexpr MediaCommand controls[] = {MediaCommand::previous, MediaCommand::toggle, MediaCommand::next};

class App {
public:
    int run(HINSTANCE instance);

private:
    static LRESULT CALLBACK dispatch(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT handle(UINT message, WPARAM wparam, LPARAM lparam);
    void draw();
    void setMode();
    void refreshPower();
    void refreshMotion();
    void addTray();
    void showMenu(POINT point);

    HWND window_ = nullptr;
    HPOWERNOTIFY powerNotification_ = nullptr;
    UINT taskbarCreated_ = 0;
    UINT renderInterval_ = 0;
    NOTIFYICONDATAW tray_{};
    std::shared_ptr<MediaMonitor> monitor_;
    MediaState media_;
    PowerState power_;
    Island island_;
    IslandMode mode_ = IslandMode::dot;
    bool showPower_ = false;
    bool animate_ = true;
    bool tracking_ = false;
    int hovered_ = -1;
    int pressed_ = -1;
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
    powerNotification_ = RegisterPowerSettingNotification(window_, &GUID_ACDC_POWER_SOURCE,
                                                           DEVICE_NOTIFY_WINDOW_HANDLE);
    SetTimer(window_, powerTimer, 10'000, nullptr);
    addTray();
    draw();
    ShowWindow(window_, SW_SHOWNOACTIVATE);
    monitor_ = std::make_shared<MediaMonitor>(window_, [this](MediaState const& state) {
        if (state.playing && !media_.playing) showPower_ = false;
        media_ = state;
        setMode();
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

void App::draw() {
    island_.draw(window_, media_, power_, animate_, hovered_, pressed_);
    UINT interval = 0;
    if (animate_ && (island_.animating() || (mode_ == IslandMode::media && media_.playing))) {
        interval = 33;
    } else if (mode_ == IslandMode::media && media_.duration > 0) {
        interval = 1000;
    }
    if (interval == renderInterval_) return;
    if (renderInterval_ != 0) KillTimer(window_, renderTimer);
    renderInterval_ = interval;
    if (renderInterval_ != 0) SetTimer(window_, renderTimer, renderInterval_, nullptr);
}

void App::setMode() {
    const auto next = showPower_ ? IslandMode::power : (media_.playing ? IslandMode::media : IslandMode::dot);
    if (next != mode_) {
        mode_ = next;
        hovered_ = pressed_ = -1;
        island_.setMode(mode_, animate_);
    }
    draw();
}

void App::refreshPower() {
    const auto state = readPower();
    if (power_ == state) return;
    power_ = state;
    if (mode_ == IslandMode::power) draw();
}

void App::refreshMotion() {
    BOOL animations = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0);
    animate_ = animations != FALSE;
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
    AppendMenuW(menu, MF_STRING, showBattery, showPower_ ? L"Hide &battery status" : L"Show &battery status");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
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
    } else if (selected == showBattery) {
        showPower_ = !showPower_;
        setMode();
    } else if (selected >= previousTrack && selected <= nextTrack && monitor_) {
        monitor_->control(controls[selected - previousTrack]);
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
        draw();
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
        draw();
        return 0;
    case WM_POWERBROADCAST:
        refreshPower();
        return TRUE;
    case WM_TIMER:
        if (wparam == powerTimer) refreshPower();
        if (wparam == renderTimer) draw();
        return 0;
    case MediaMonitor::notification:
        if (monitor_) monitor_->refresh(wparam);
        return 0;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_MOUSEMOVE: {
        if (!tracking_) {
            TRACKMOUSEEVENT tracking{sizeof(TRACKMOUSEEVENT), TME_LEAVE, window_, 0};
            tracking_ = TrackMouseEvent(&tracking) != FALSE;
        }
        const int hit = island_.buttonAt(window_, {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        if (hovered_ != hit) {
            hovered_ = hit;
            draw();
        }
        SetCursor(LoadCursorW(nullptr, hit >= 0 || mode_ != IslandMode::media ? IDC_HAND : IDC_ARROW));
        return 0;
    }
    case WM_MOUSELEAVE:
        tracking_ = false;
        hovered_ = -1;
        draw();
        return 0;
    case WM_LBUTTONDOWN:
        pressed_ = island_.buttonAt(window_, {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        if (pressed_ >= 0) {
            SetCapture(window_);
            draw();
        }
        return 0;
    case WM_LBUTTONUP: {
        const int pressed = pressed_;
        if (GetCapture() == window_) ReleaseCapture();
        if (mode_ != IslandMode::media) {
            showPower_ = !showPower_;
            setMode();
        } else if (pressed >= 0 && monitor_ &&
                   pressed == island_.buttonAt(window_, {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)})) {
            monitor_->control(controls[pressed]);
        }
        return 0;
    }
    case WM_CAPTURECHANGED:
        pressed_ = -1;
        draw();
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
        KillTimer(window_, renderTimer);
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
