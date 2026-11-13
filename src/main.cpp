#include <initguid.h>

#include "bluetooth.hpp"
#include "browser_tabs.hpp"
#include "desktop.hpp"
#include "drop_zone.hpp"
#include "island.hpp"
#include "notifications.hpp"
#include "resource.h"
#include "settings.hpp"
#include "settings_example.hpp"
#include <commctrl.h>
#include <hidusage.h>
#include <ole2.h>
#include <set>
#include <shellapi.h>
#include <utility>
#include <windowsx.h>
#include <winrt/Windows.Networking.Connectivity.h>

namespace zenith {
namespace {

using namespace std::chrono_literals;

constexpr wchar_t appName[] = L"Zenula";
constexpr UINT trayMessage = WM_APP + 2;
constexpr UINT redrawMessage = WM_APP + 3;
constexpr UINT networkChangedMessage = WM_APP + 21;
constexpr UINT timerEditFinished = WM_APP + 22;
constexpr UINT_PTR powerTimer = 1;
constexpr UINT_PTR frameTimer = 2;
constexpr UINT_PTR fullscreenTimer = 3;
constexpr UINT_PTR clipboardTimer = 5;
constexpr UINT_PTR networkTimer = 6;
static_assert(clipboardTimer != MediaMonitor::artworkTimer);
constexpr UINT clipboardRetryInterval = 40;
constexpr unsigned clipboardRetryLimit = 25;

enum Menu { openSettings = 1, toggleVisibility, openDropZone, exitApp };

struct LockKey {
    USHORT virtualKey;
    IslandAlert::Kind kind;
    wchar_t const* label;
    bool enabled = false;
    std::set<HANDLE> held{};
};

class App {
public:
    int run(HINSTANCE instance, std::optional<SettingsTest> test, Feature feature);

private:
    static LRESULT CALLBACK dispatch(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT handle(UINT message, WPARAM wparam, LPARAM lparam);
    void frame();
    void scheduleFrames(UINT interval);
    void startSettingsTest(SettingsTest test, Feature feature);
    void updateMedia();
    void control(MediaCommand command);
    void run(IslandCommand const& command);
    void openTimerEdit();
    void closeTimerEdit(bool commit, bool restoreFocus = false);
    static LRESULT CALLBACK timerEditInput(HWND edit, UINT message, WPARAM wparam, LPARAM lparam,
                                          UINT_PTR id, DWORD_PTR context);
    void refreshPower();
    void queueNetworkRefresh();
    void startNetworkRefresh();
    winrt::Windows::Foundation::IAsyncAction refreshNetwork();
    void applyNetwork(NetworkStatus status);
    bool refreshPrayers();
    winrt::Windows::Foundation::IAsyncAction loadPrayerMonth(unsigned city, std::chrono::year_month month);
    void cancelPrayerRequest();
    void refreshMotion();
    bool refreshFullscreen();
    void refreshPillOverlap();
    winrt::Windows::Foundation::IAsyncAction checkPillOverlap(HWND behind, RECT footprint);
    void applyVisibility();
    void maintainZOrder();
    bool suppressed() const { return fullscreen_ || hidden_; }
    void refreshCapture(bool announce = true);
    void announceBluetooth();
    void addTray();
    void showMenu(POINT point);
    void launchSettings();
    void queueClipboard(bool listenerEvent = false);
    void refreshClipboard();
    void updateLockKey(RAWKEYBOARD const& keyboard, HANDLE device);
    std::optional<IslandAlert> clipboardAlert(bool& busy, HBITMAP& image, std::wstring& stagedText);

    HWND window_ = nullptr;
    HWND timerEdit_ = nullptr;
    HWND timerPreviousFocus_ = nullptr;
    HFONT timerEditFont_ = nullptr;
    HPOWERNOTIFY powerNotification_ = nullptr;
    UINT taskbarCreated_ = 0;
    UINT frameInterval_ = 0;
    NOTIFYICONDATAW tray_{};
    HICON trayIcon_ = nullptr;
    std::shared_ptr<MediaMonitor> monitor_;
    std::shared_ptr<Lyrics> lyrics_;
    MediaState media_;
    MediaState nativeMedia_;
    AudioSpectrum spectrum_;
    BrowserTabs browserTabs_;
    PowerState power_;
    DesktopMonitor desktop_;
    Notifications notifications_;
    BluetoothMonitor bluetooth_;
    SystemMonitor system_;
    NetworkStatus network_;
    std::optional<bool> lastOnline_;
    winrt::Windows::Networking::Connectivity::NetworkInformation::NetworkStatusChanged_revoker networkChanged_;
    winrt::Windows::Foundation::IAsyncAction networkRequest_{nullptr};
    winrt::Windows::Foundation::IAsyncAction overlapRequest_{nullptr};
    Island::Clock::time_point nextOverlapCheck_{};
    Scratchpad scratchpad_;
    DropZone dropZone_;
    Settings settings_;
    unsigned prayerCity_ = 0;
    unsigned announcedPrayers_ = 0;
    std::optional<PrayerSchedule> prayers_;
    PrayerMonth prayerMonth_, nextPrayerMonth_;
    winrt::Windows::Foundation::IAsyncAction prayerRequest_{nullptr};
    Island::Clock::time_point prayerRetryAt_{};
    std::chrono::sys_days prayerReminderDate_{};
    bool prayerPickerActive_ = false;
    std::chrono::steady_clock::time_point systemSampled_{};
    Island island_;
    std::optional<SettingsExample> example_;
    bool tracking_ = false;
    bool trayMenuOpen_ = false;
    bool fullscreen_ = false;
    bool hidden_ = false;
    bool clipboardListening_ = false;
    DWORD clipboardSequence_ = 0;
    DWORD stagedImageSequence_ = 0;
    DWORD stagedTextSequence_ = 0;
    std::optional<unsigned> clipboardAttempts_;
    unsigned captureTick_ = 0;
    unsigned lowWarned_ = 0;   // the low-battery threshold (20 or 10) already announced this discharge
    LockKey lockKeys_[2]{
        {VK_CAPITAL, IslandAlert::Kind::capsLock, L"Caps Lock"},
        {VK_NUMLOCK, IslandAlert::Kind::numLock, L"Num Lock"},
    };
};

int App::run(HINSTANCE instance, std::optional<SettingsTest> test, Feature feature) {
    settings_ = Settings::load();
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
    island_.setSystem(system_.sample(false));
    island_.setPreferences(settings_);
    dropZone_.setEnabled(settings_.enabled(Feature::drop));
    powerNotification_ = RegisterPowerSettingNotification(window_, &GUID_ACDC_POWER_SOURCE,
                                                           DEVICE_NOTIFY_WINDOW_HANDLE);
    SetTimer(window_, powerTimer, 10'000, nullptr);
    SetTimer(window_, fullscreenTimer, 750, nullptr);
    clipboardSequence_ = GetClipboardSequenceNumber();
    clipboardListening_ = AddClipboardFormatListener(window_) != FALSE;
    for (auto& key : lockKeys_) key.enabled = (GetKeyState(key.virtualKey) & 1) != 0;
    const RAWINPUTDEVICE keyboard{HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_KEYBOARD, RIDEV_INPUTSINK, window_};
    if (!RegisterRawInputDevices(&keyboard, 1, sizeof(keyboard))) {
        OutputDebugStringW(L"Zenula could not register lock key input.\n");
    }
    dropZone_.attach(window_, [this](bool hovering) {
        island_.setDropHover(hovering);
        frame();
    }, [this](std::vector<DropPreview> items) {
        island_.setDropItems(std::move(items));
        frame();
    });
    desktop_.start(window_);
    notifications_.refresh(window_, settings_.showAppNotifications);
    bluetooth_.start(window_);
    try {
        using winrt::Windows::Networking::Connectivity::NetworkInformation;
        networkChanged_ = NetworkInformation::NetworkStatusChanged(winrt::auto_revoke, [window = window_](auto const&) {
            PostMessageW(window, networkChangedMessage, 0, 0);
        });
    } catch (winrt::hresult_error const&) {
        OutputDebugStringW(L"Zenula could not subscribe to network status changes.\n");
    }
    startNetworkRefresh();
    island_.setAudioOutput(desktop_.audioOutput());
    island_.setWindowsNotificationsQuiet(desktop_.notificationsQuiet());
    refreshPrayers();
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
    if (test) startSettingsTest(*test, feature);

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
    const auto now = Island::Clock::now();
    if (example_) {
        if (!example_->finished(now)) {
            example_->draw(window_, now);
            if (!IsWindowVisible(window_)) {
                ShowWindow(window_, SW_SHOWNOACTIVATE);
                maintainZOrder();
            }
            scheduleFrames(example_->frameInterval(now));
            return;
        }
        example_.reset();
        applyVisibility();
    }
    island_.tick();
    const bool showSpectrum = !suppressed() && island_.wantsAudioSpectrum();
    spectrum_.select(showSpectrum ? std::wstring_view(media_.appId) : std::wstring_view{});
    if (showSpectrum) island_.setSpectrum(spectrum_.levels());
    if (island_.wantsPrayerClock()) refreshPrayers();
    if (scratchpad_.window() && !island_.scratchEditorRect()) {
        scratchpad_.close();
        island_.setScratchEditing(false);
    }
    if (lyrics_) lyrics_->update(media_, island_.wantsLyrics(), island_.wantsLyricPill());
    if (island_.wantsSystemStats() && now - systemSampled_ >= 1s) {
        island_.setSystem(system_.sample());
        island_.setAppVolumes(desktop_.appVolumes());
        island_.setAudioOutputs(desktop_.audioOutputs());
        island_.setBluetoothDevices(bluetooth_.connectedDevices());
        systemSampled_ = now;
    } else if (!island_.wantsSystemStats() && systemSampled_ != std::chrono::steady_clock::time_point{}) {
        system_.stopExtended();
        systemSampled_ = {};
    }
    const bool shown = IsWindowVisible(window_) != FALSE;
    const bool visible = island_.visible() && (shown || !suppressed());
    if (visible) island_.draw(window_);
    if (timerEdit_) {
        if (const auto bounds = island_.timerEditorRect(); visible && bounds) {
            RECT current{};
            GetWindowRect(timerEdit_, &current);
            if (!EqualRect(&current, &*bounds))
                SetWindowPos(timerEdit_, nullptr, bounds->left, bounds->top, bounds->right - bounds->left,
                             bounds->bottom - bounds->top, SWP_NOACTIVATE | SWP_NOZORDER);
        } else {
            closeTimerEdit(false);
        }
    }
    if (scratchpad_.window()) {
        if (const auto bounds = island_.scratchEditorRect()) scratchpad_.place(*bounds);
    }
    if (visible != shown) {
        ShowWindow(window_, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
        if (visible) maintainZOrder();
    }
    scheduleFrames(island_.frameInterval());
    refreshPillOverlap();
}

void App::scheduleFrames(UINT interval) {
    if (interval == frameInterval_) return;
    if (frameInterval_ != 0) KillTimer(window_, frameTimer);
    frameInterval_ = interval;
    if (frameInterval_ != 0) SetTimer(window_, frameTimer, frameInterval_, nullptr);
}

void App::startSettingsTest(SettingsTest test, Feature feature) {
    BOOL animations = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0);
    if (GetCapture() == window_) ReleaseCapture();
    spectrum_.select({});
    example_.emplace(test, feature, settings_, animations != FALSE);
    applyVisibility();
    frame();
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

void App::openTimerEdit() {
    const auto bounds = island_.timerEditorRect();
    if (!bounds || timerEdit_) return;
    timerPreviousFocus_ = GetForegroundWindow();
    const int height = bounds->bottom - bounds->top;
    timerEdit_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"EDIT", island_.timerDurationText().c_str(),
        WS_POPUP | WS_BORDER | ES_CENTER | ES_AUTOHSCROLL, bounds->left, bounds->top,
        bounds->right - bounds->left, height, window_, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!timerEdit_) return;
    if (!SetWindowSubclass(timerEdit_, timerEditInput, 1, reinterpret_cast<DWORD_PTR>(window_))) {
        closeTimerEdit(false);
        return;
    }
    timerEditFont_ = CreateFontW(-height * 2 / 3, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    SendMessageW(timerEdit_, WM_SETFONT, reinterpret_cast<WPARAM>(timerEditFont_), FALSE);
    SendMessageW(timerEdit_, EM_SETLIMITTEXT, 6, 0);
    island_.setPopupOpen(true);
    ShowWindow(timerEdit_, SW_SHOW);
    SetForegroundWindow(timerEdit_);
    SetFocus(timerEdit_);
    SendMessageW(timerEdit_, EM_SETSEL, 0, -1);
}

void App::closeTimerEdit(bool commit, bool restoreFocus) {
    if (!timerEdit_) return;
    const HWND closing = std::exchange(timerEdit_, nullptr);
    if (commit && island_.timerEditorRect()) {
        wchar_t input[7]{};
        GetWindowTextW(closing, input, static_cast<int>(std::size(input)));
        island_.setTimerDurationText(input);
    }
    const bool restore = restoreFocus && GetForegroundWindow() == closing;
    RemoveWindowSubclass(closing, timerEditInput, 1);
    DestroyWindow(closing);
    if (timerEditFont_) DeleteObject(std::exchange(timerEditFont_, nullptr));
    island_.setPopupOpen(false);
    if (restore && IsWindow(timerPreviousFocus_)) SetForegroundWindow(timerPreviousFocus_);
    timerPreviousFocus_ = nullptr;
}

LRESULT CALLBACK App::timerEditInput(HWND edit, UINT message, WPARAM wparam, LPARAM lparam,
                                    UINT_PTR, DWORD_PTR context) {
    const auto owner = reinterpret_cast<HWND>(context);
    if (message == WM_KEYDOWN && (wparam == VK_RETURN || wparam == VK_ESCAPE)) {
        PostMessageW(owner, timerEditFinished, reinterpret_cast<WPARAM>(edit), wparam == VK_RETURN ? 2 : 0);
        return 0;
    }
    if (message == WM_CHAR && wparam >= 32 && wparam != L':' && (wparam < L'0' || wparam > L'9')) return 0;
    if (message == WM_KILLFOCUS) PostMessageW(owner, timerEditFinished, reinterpret_cast<WPARAM>(edit), 1);
    return DefSubclassProc(edit, message, wparam, lparam);
}

void App::run(IslandCommand const& command) {
    switch (command.kind) {
    case IslandCommand::Kind::openSettings: launchSettings(); break;
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
    case IslandCommand::Kind::openSource:
        if (!media_.sourceId.empty()) browserTabs_.activate(media_.sourceId);
        else activateMediaWindow(window_, media_.appId);
        break;
    case IslandCommand::Kind::retryLyrics: if (lyrics_) lyrics_->retry(); break;
    case IslandCommand::Kind::adjustLyricsDelay:
        if (lyrics_) lyrics_->adjustDelay(command.index == 0 ? -1 : 1);
        break;
    case IslandCommand::Kind::editTimerDuration: openTimerEdit(); break;
    case IslandCommand::Kind::editScratchpad:
        if (const auto bounds = island_.scratchEditorRect()) {
            island_.setScratchEditing(scratchpad_.open(window_, *bounds));
        }
        break;
    case IslandCommand::Kind::selectAudioOutput:
        if (!desktop_.selectAudioOutput(command.appId)) {
            MessageBoxW(window_, L"Windows couldn't switch to that audio output.", appName, MB_OK | MB_ICONWARNING);
        }
        desktop_.refreshAudioDevice();
        island_.setAudioOutput(desktop_.audioOutput());
        island_.setAudioOutputs(desktop_.audioOutputs());
        systemSampled_ = {};
        break;
    case IslandCommand::Kind::soundSettings:
        ShellExecuteW(window_, L"open", L"ms-settings:sound", nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case IslandCommand::Kind::clearDrop: dropZone_.clear(); break;
    case IslandCommand::Kind::removeDrop: dropZone_.remove(command.index); break;
    case IslandCommand::Kind::openDropImage:
        if (dropZone_.openImage(command.index, window_)) island_.setPopupOpen(true);
        break;
    case IslandCommand::Kind::notificationSettings:
        ShellExecuteW(window_, L"open", L"ms-settings:notifications", nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case IslandCommand::Kind::appVolume:
        // Moving the slider unmutes, as in the Windows volume mixer.
        desktop_.setAppVolume(command.appId, static_cast<float>(command.seconds), false);
        island_.setAppVolumes(desktop_.appVolumes());
        break;
    case IslandCommand::Kind::autoCycle: {
        auto updated = settings_;
        updated.autoCycle = command.seconds != 0;
        if (updated.saveTimer()) settings_ = updated;
        break;
    }
    case IslandCommand::Kind::prayerLocation: {
        prayerPickerActive_ = true;
        island_.setPopupOpen(true);
        const auto city = choosePrayerLocation(window_, settings_.prayerCity);
        island_.setPopupOpen(false);
        prayerPickerActive_ = false;
        if (city && *city != settings_.prayerCity) {
            auto updated = Settings::load();
            updated.prayerCity = *city;
            if (updated.saveNotifications()) {
                settings_.prayerCity = updated.prayerCity;
                island_.setPreferences(settings_);
                refreshPrayers();
            } else MessageBoxW(window_, L"Could not save the prayer location.", appName, MB_OK | MB_ICONWARNING);
        }
        break;
    }
    case IslandCommand::Kind::appMute:
        desktop_.setAppVolume(command.appId, std::nullopt, command.seconds != 0);
        island_.setAppVolumes(desktop_.appVolumes());
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

void App::queueNetworkRefresh() {
    if (networkRequest_) networkRequest_.Cancel();
    if (!SetTimer(window_, networkTimer, 1200, nullptr)) startNetworkRefresh();
}

void App::startNetworkRefresh() {
    if (networkRequest_) networkRequest_.Cancel();
    networkRequest_ = refreshNetwork();
}

winrt::Windows::Foundation::IAsyncAction App::refreshNetwork() {
    const auto cancellation = co_await winrt::get_cancellation_token();
    const winrt::apartment_context ui;
    co_await winrt::resume_background();
    auto status = readNetwork();
    if (cancellation()) co_return;
    co_await ui;
    if (!cancellation()) applyNetwork(std::move(status));
}

void App::applyNetwork(NetworkStatus status) {
    if (network_ == status) return;
    network_ = std::move(status);
    island_.setNetwork(network_);
    const auto online = updateInternetStatus(lastOnline_, network_.access);
    if (online && !suppressed()) {
        island_.notify({IslandAlert::Kind::connection, *online ? L"Back online" : L"Connection lost",
                        L"", *online ? 0x91caffu : 0xff9f0au, 2});
        frame();
    } else if (island_.wantsSystemStats()) {
        frame();
    }
}

void App::cancelPrayerRequest() {
    if (prayerRequest_) prayerRequest_.Cancel();
    prayerRequest_ = nullptr;
}

winrt::Windows::Foundation::IAsyncAction App::loadPrayerMonth(unsigned city, std::chrono::year_month month) {
    const auto cancel = co_await winrt::get_cancellation_token();
    cancel.enable_propagation();
    const winrt::apartment_context ui;
    co_await winrt::resume_background();
    const auto result = std::make_shared<PrayerMonth>();
    try {
        co_await fetchPrayerMonth(city, month, result);
    } catch (winrt::hresult_error const&) {
        if (cancel()) co_return;
    }
    if (cancel()) co_return;
    co_await ui;
    if (cancel() || city != prayerCity_) co_return;

    if (result->city) {
        const std::chrono::year_month_day date{prayerDate(city, std::chrono::system_clock::now())};
        auto& slot = month == date.year() / date.month() ? prayerMonth_ : nextPrayerMonth_;
        slot = std::move(*result);
    }
    const std::chrono::year_month_day today{prayerDate(city, std::chrono::system_clock::now())};
    const auto current = today.year() / today.month();
    const bool available = month == current
        ? prayerMonth_.month == month && prayerMonth_.days[unsigned(today.day()) - 1].has_value()
        : nextPrayerMonth_.month == month && nextPrayerMonth_.days.front().has_value();
    prayerRetryAt_ = available ? Island::Clock::time_point{} : Island::Clock::now() + 5min;
    island_.setPrayerLoad(PrayerLoad::unavailable);
    frame();
}

bool App::refreshPrayers() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto date = prayerDate(settings_.prayerCity, now);
    if (prayerCity_ != settings_.prayerCity) {
        prayerCity_ = settings_.prayerCity;
        prayerMonth_ = nextPrayerMonth_ = {};
        prayerReminderDate_ = {};
        prayerRetryAt_ = {};
        cancelPrayerRequest();
    }
    if (prayerReminderDate_ != date) {
        prayerReminderDate_ = date;
        announcedPrayers_ = 0;
    }
    if (prayerCity_) {
        const year_month_day today{date};
        const auto month = today.year() / today.month();
        if (nextPrayerMonth_.city && nextPrayerMonth_.month == month) {
            prayerMonth_ = std::exchange(nextPrayerMonth_, {});
            prayerRetryAt_ = {};
        }
        const bool missingToday = prayerMonth_.month != month || !prayerMonth_.days[unsigned(today.day()) - 1];
        const bool lastDay = today.day() == year_month_day_last{month / last}.day();
        const auto next = month + months{1};
        const bool missingTomorrow = lastDay && (nextPrayerMonth_.month != next || !nextPrayerMonth_.days.front());
        if ((missingToday || missingTomorrow) && Island::Clock::now() >= prayerRetryAt_ &&
            (!prayerRequest_ || prayerRequest_.Status() != winrt::Windows::Foundation::AsyncStatus::Started)) {
            if (missingToday) island_.setPrayerLoad(PrayerLoad::loading);
            prayerRequest_ = loadPrayerMonth(prayerCity_, missingToday ? month : next);
        }
    }
    const auto updated = prayerSchedule(prayerMonth_, date, nextPrayerMonth_);
    const bool changed = prayers_ != updated;
    if (changed) {
        prayers_ = updated;
        island_.setPrayerSchedule(prayers_);
    }
    if (!prayers_) return changed;
    const auto local = now + std::chrono::hours(prayers_->utcOffset);
    const auto minute = std::chrono::duration_cast<std::chrono::minutes>(local - date).count();
    for (size_t i = 0; i < prayers_->times.size(); ++i) {
        const unsigned bit = 1u << i;
        if (minute != prayers_->times[i] || (announcedPrayers_ & bit)) continue;
        announcedPrayers_ |= bit;
        if (settings_.prayerReminders && !suppressed()) {
            island_.notify({IslandAlert::Kind::prayer, prayerNames[i], prayers_->city, std::nullopt, 3});
            return true;
        }
    }
    return changed;
}

void App::refreshMotion() {
    BOOL animations = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0);
    island_.setAnimate(animations != FALSE);
}

bool App::refreshFullscreen() {
    if (prayerPickerActive_) return false;
    const auto foreground = GetForegroundWindow();
    if (settings_.hideInFullscreen && foreground &&
        (foreground == window_ || foreground == scratchpad_.window())) return false;
    const bool fullscreen = settings_.hideInFullscreen &&
        (!foreground || foreground != dropZone_.previewWindow()) && fullscreenOnPrimary(window_, foreground);
    if (fullscreen == fullscreen_) return false;
    fullscreen_ = fullscreen;
    applyVisibility();
    return true;
}

void App::refreshPillOverlap() {
    if (!settings_.shrinkOverControls) {
        if (island_.setPillOccluded(false)) frame();
        return;
    }
    if (suppressed() || example_ || prayerPickerActive_) return;
    const auto now = Island::Clock::now();
    if (now < nextOverlapCheck_ ||
        (overlapRequest_ && overlapRequest_.Status() == winrt::Windows::Foundation::AsyncStatus::Started)) return;
    const auto footprint = island_.pillFootprint();
    if (!footprint) {
        if (island_.setPillOccluded(false)) frame();
        return;
    }
    nextOverlapCheck_ = now + 1500ms;
    if (const auto behind = windowBehindPill(window_, *footprint)) overlapRequest_ = checkPillOverlap(behind, *footprint);
    else if (island_.setPillOccluded(false)) frame();
}

winrt::Windows::Foundation::IAsyncAction App::checkPillOverlap(HWND behind, RECT footprint) {
    const auto cancellation = co_await winrt::get_cancellation_token();
    const winrt::apartment_context ui;
    co_await winrt::resume_background();
    const bool occluded = windowControlsOverlap(behind, footprint);
    if (cancellation()) co_return;
    co_await ui;
    if (cancellation() || !settings_.shrinkOverControls || suppressed() || example_) co_return;

    const auto current = island_.pillFootprint();
    if (current && EqualRect(&*current, &footprint) && windowBehindPill(window_, *current) == behind &&
        island_.setPillOccluded(occluded)) frame();
}

void App::maintainZOrder() {
    if (!suppressed() && !trayMenuOpen_) restoreOverlayZOrder(window_);
}

void App::applyVisibility() {
    const bool hide = suppressed();
    island_.setSuppressed(hide);
    const auto style = GetWindowLongPtrW(window_, GWL_EXSTYLE);
    SetWindowLongPtrW(window_, GWL_EXSTYLE, hide || example_ ? style | WS_EX_TRANSPARENT : style & ~WS_EX_TRANSPARENT);
    if (hide && GetCapture() == window_) ReleaseCapture();
}

void App::refreshCapture(bool announce) {
    auto activity = desktop_.captureActivity();
    if (!settings_.showMicrophoneIndicator) activity.microphone.clear();
    if (!settings_.showCameraIndicator) activity.camera.clear();
    if (island_.setCaptureActivity(activity) && announce) frame();
}

void App::announceBluetooth() {
    auto events = bluetooth_.take();
    if (events.empty()) return;
    for (auto& event : events) {
        IslandAlert alert{event.audio ? IslandAlert::Kind::bluetoothAudio : IslandAlert::Kind::bluetooth,
                          std::move(event.name)};
        alert.priority = 2;
        if (!event.connected) {
            alert.value = L"Disconnected";
            alert.color = 0xa4aab4u;
        } else if (event.battery) {
            alert.value = std::to_wstring(*event.battery) + L"%";
            alert.battery = event.battery;
            if (*event.battery <= 10) alert.color = 0xff453au;
            else if (*event.battery <= 20) alert.color = 0xff9f0au;
        } else {
            alert.value = L"Connected";
        }
        island_.notify(std::move(alert));
    }
    frame();
}

void App::addTray() {
    tray_.cbSize = sizeof(tray_);
    tray_.hWnd = window_;
    tray_.uID = 1;
    tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    tray_.uCallbackMessage = trayMessage;
    if (!trayIcon_) {
        trayIcon_ = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_ZENITH_DOT),
                                                IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                                GetSystemMetrics(SM_CYSMICON), 0));
    }
    tray_.hIcon = trayIcon_ ? trayIcon_ : LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(tray_.szTip, appName);
    if (Shell_NotifyIconW(NIM_ADD, &tray_)) {
        tray_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &tray_);
    }
}

void App::showMenu(POINT point) {
    const auto menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, openSettings, L"&Settings…");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, toggleVisibility, hidden_ ? L"&Show Zenula" : L"&Hide Zenula");
    AppendMenuW(menu, MF_STRING | (suppressed() || !settings_.enabled(Feature::drop) ? MF_GRAYED : MF_ENABLED),
                openDropZone, L"&Drop Zone");
    AppendMenuW(menu, MF_STRING, exitApp, L"E&xit Zenula");
    // TrackPopupMenu needs the owner in the foreground to dismiss correctly. Hand focus back afterwards.
    const auto previous = GetForegroundWindow();
    trayMenuOpen_ = true;
    SetForegroundWindow(window_);
    const auto selected = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                                         point.x, point.y, 0, window_, nullptr);
    trayMenuOpen_ = false;
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
    } else if (selected == openSettings) {
        launchSettings();
    }
}

void App::launchSettings() {
    std::wstring executable(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (!length || length >= executable.size() ||
        reinterpret_cast<INT_PTR>(ShellExecuteW(window_, L"open", executable.c_str(), L"--settings",
                                               nullptr, SW_SHOWNORMAL)) <= 32) {
        MessageBoxW(window_, L"Couldn’t open Zenula Settings.", appName, MB_OK | MB_ICONWARNING);
    }
}


// Private clipboard formats are excluded before either previewing text or staging an image.
std::optional<IslandAlert> App::clipboardAlert(bool& busy, HBITMAP& image, std::wstring& stagedText) {
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
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT) && !IsClipboardFormatAvailable(CF_HDROP) &&
        (IsClipboardFormatAvailable(CF_BITMAP) || IsClipboardFormatAvailable(CF_DIB) ||
         IsClipboardFormatAvailable(CF_DIBV5))) {
        alert.value = L"Image";
        if (!settings_.autoAddScreenshots || !settings_.enabled(Feature::drop)) return alert;
        const auto bitmap = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
        const DWORD sequence = GetClipboardSequenceNumber();
        if (sequence && sequence == stagedImageSequence_) return std::nullopt;
        BITMAP size{};
        if (bitmap && GetObjectW(bitmap, sizeof(size), &size) && size.bmWidth > 0 && size.bmHeight > 0 &&
            static_cast<uint64_t>(size.bmWidth) * size.bmHeight <= 32'000'000) {
            image = static_cast<HBITMAP>(CopyImage(bitmap, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
        }
        return alert;
    }
    if (suppressed() && !settings_.autoAddText) return std::nullopt;
    if (const auto data = GetClipboardData(CF_UNICODETEXT)) {
        const bool showPreview = settings_.showClipboardBubble && settings_.showClipboardPreviews && !suppressed();
        if (!settings_.autoAddText && !showPreview) return alert;
        if (const auto text = static_cast<wchar_t const*>(GlobalLock(data))) {
            struct Unlock {
                HGLOBAL data;
                ~Unlock() { GlobalUnlock(data); }
            } unlock{data};
            std::wstring_view all(text, wcsnlen(text, GlobalSize(data) / sizeof(wchar_t)));
            const DWORD sequence = GetClipboardSequenceNumber();
            if (settings_.autoAddText && settings_.enabled(Feature::drop) &&
                (!sequence || sequence != stagedTextSequence_) && dropZone_.canAddText(all.size())) {
                stagedText.assign(all);
            }
            if (!showPreview) return alert;
            // First non-empty line, trimmed, at most 60 characters.
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
        }
    } else if (const auto drop = suppressed() ? nullptr : static_cast<HDROP>(GetClipboardData(CF_HDROP))) {
        const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        wchar_t name[MAX_PATH]{};
        if (count == 1 && DragQueryFileW(drop, 0, name, MAX_PATH)) {
            const auto slash = std::wstring_view(name).find_last_of(L"\\/");
            alert.value = slash == std::wstring_view::npos ? name : name + slash + 1;
        } else if (count > 1) {
            alert.value = std::to_wstring(count) + L" files";
        }
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
    if (hidden_) {
        clipboardSequence_ = GetClipboardSequenceNumber();
        KillTimer(window_, clipboardTimer);
        clipboardAttempts_.reset();
        return;
    }
    bool busy = false;
    struct Bitmap {
        HBITMAP value = nullptr;
        ~Bitmap() { if (value) DeleteObject(value); }
    } image;
    std::wstring stagedText;
    auto alert = clipboardAlert(busy, image.value, stagedText);
    if (busy && ++*clipboardAttempts_ < clipboardRetryLimit) {
        if (SetTimer(window_, clipboardTimer, clipboardRetryInterval, nullptr)) return;
    }
    KillTimer(window_, clipboardTimer);
    clipboardAttempts_.reset();
    if (alert) {
        if (!stagedText.empty() && dropZone_.addText(stagedText)) stagedTextSequence_ = clipboardSequence_;
        if (image.value) {
            const auto result = dropZone_.addImage(image.value);
            if (result != DropZone::ImageResult::unavailable) stagedImageSequence_ = clipboardSequence_;
            if (result == DropZone::ImageResult::duplicate) return;
            if (result == DropZone::ImageResult::added) alert->value = L"Image in Drop Zone";
        }
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
        maintainZOrder();
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
        island_.setWindowsNotificationsQuiet(desktop_.notificationsQuiet());
        [[fallthrough]];
    case WM_DISPLAYCHANGE:
    case WM_DPICHANGED:
        PostMessageW(window_, redrawMessage, 0, 0);
        return 0;
    case redrawMessage:
        refreshFullscreen();
        frame();
        maintainZOrder();
        return 0;
    case WM_POWERBROADCAST:
        if (wparam == PBT_APMRESUMEAUTOMATIC) queueNetworkRefresh();
        refreshPower();
        return TRUE;
    case networkChangedMessage:
        queueNetworkRefresh();
        return 0;
    case WM_TIMER:
        if (wparam == MediaMonitor::artworkTimer && monitor_) monitor_->retryArtwork();
        if (wparam == networkTimer) {
            KillTimer(window_, networkTimer);
            startNetworkRefresh();
        }
        if (wparam == powerTimer) {
            if (!networkChanged_) startNetworkRefresh();
            if (!clipboardListening_) clipboardListening_ = AddClipboardFormatListener(window_) != FALSE;
            const bool changed = island_.setWindowsNotificationsQuiet(desktop_.notificationsQuiet());
            notifications_.refresh(window_, settings_.showAppNotifications);
            const bool prayerChanged = refreshPrayers();
            refreshPower();
            if (changed || prayerChanged) frame();
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
            refreshPillOverlap();
            maintainZOrder();
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
        nextOverlapCheck_ = {};
        if (refreshFullscreen()) frame();
        else refreshPillOverlap();
        maintainZOrder();
        return 0;
    case DropZone::previewClosed:
        if (!dropZone_.previewWindow()) {
            island_.setPopupOpen(false);
            refreshFullscreen();
            island_.showDropZone(true);
            frame();
        }
        return 0;
    case DesktopMonitor::captureChanged:
        refreshCapture();
        return 0;
    case BluetoothMonitor::changed:
        announceBluetooth();
        return 0;
    case DesktopMonitor::audioDeviceChanged:
        systemSampled_ = {};
        desktop_.refreshAudioDevice();
        island_.setAudioOutput(desktop_.audioOutput());
        frame();
        return 0;
    case notificationArrivedMessage:
        if (settings_.showAppNotifications && !suppressed() && !island_.alertsQuiet()) {
            if (auto notification = notifications_.read(static_cast<unsigned>(wparam))) {
                island_.notify({IslandAlert::Kind::notification, std::move(notification->app),
                               std::move(notification->title), std::nullopt, 2, 2500ms});
                frame();
            }
        }
        return 0;
    case DesktopMonitor::notificationModeChanged:
        if (island_.setWindowsNotificationsQuiet(desktop_.notificationsQuiet())) frame();
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
        if (suppressed() || example_) return 0;
        if (!tracking_) {
            TRACKMOUSEEVENT tracking{sizeof(TRACKMOUSEEVENT), TME_LEAVE, window_, 0};
            tracking_ = TrackMouseEvent(&tracking) != FALSE;
        }
        if (island_.pointerMove({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)})) {
            if (const auto live = island_.takeLiveCommand()) run(*live);
            SetCursor(LoadCursorW(nullptr, island_.overControl() ? IDC_HAND : IDC_ARROW));
            frame();
        }
        return 0;
    case WM_MOUSELEAVE:
        tracking_ = false;
        if (example_) return 0;
        if (island_.pointerLeave()) frame();
        return 0;
    case WM_MOUSEWHEEL: {
        if (example_) return 0;
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        const int delta = GET_WHEEL_DELTA_WPARAM(wparam);
        if (scratchpad_.scrollAt(point, delta)) return 0;
        ScreenToClient(window_, &point);
        if (island_.scroll(point, delta)) frame();
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (example_) return 0;
        closeTimerEdit(true);
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (const auto item = island_.dropItemAt(point)) {
            ClientToScreen(window_, &point);
            if (DragDetect(window_, point) && IsWindow(window_)) dropZone_.drag(*item);
        } else if (island_.pointerDown(point)) {
            SetCapture(window_);
            if (const auto live = island_.takeLiveCommand()) run(*live);
        }
        frame();
        return 0;
    }
    case WM_LBUTTONUP: {
        if (example_) return 0;
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
    case timerEditFinished:
        if (timerEdit_ == reinterpret_cast<HWND>(wparam)) {
            closeTimerEdit(lparam != 0, lparam != 1);
            frame();
        }
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
    case settingsTestMessage:
        if (wparam > static_cast<unsigned>(SettingsTest::notification) || !validSettingsTest(static_cast<unsigned>(wparam)) ||
            lparam < 0 || lparam >= static_cast<LPARAM>(featureOrder.size())) return FALSE;
        startSettingsTest(static_cast<SettingsTest>(wparam), static_cast<Feature>(lparam));
        return TRUE;
    case settingsChangedMessage:
        settings_ = Settings::load();
        notifications_.refresh(window_, settings_.showAppNotifications);
        refreshMotion();
        island_.setPreferences(settings_);
        dropZone_.setEnabled(settings_.enabled(Feature::drop));
        refreshPrayers();
        refreshCapture(false);
        refreshFullscreen();
        frame();
        return 0;
    case trayMessage:
        if (LOWORD(lparam) == WM_CONTEXTMENU || LOWORD(lparam) == NIN_KEYSELECT ||
            LOWORD(lparam) == NIN_SELECT) {
            showMenu({GET_X_LPARAM(wparam), GET_Y_LPARAM(wparam)});
        }
        return 0;
    case WM_DESTROY:
        closeTimerEdit(false);
        cancelPrayerRequest();
        spectrum_.select({});
        dropZone_.detach();
        scratchpad_.close();
        KillTimer(window_, powerTimer);
        KillTimer(window_, frameTimer);
        KillTimer(window_, fullscreenTimer);
        KillTimer(window_, clipboardTimer);
        KillTimer(window_, networkTimer);
        networkChanged_.revoke();
        if (networkRequest_) networkRequest_.Cancel();
        if (overlapRequest_) overlapRequest_.Cancel();
        desktop_.stop();
        notifications_.stop();
        bluetooth_.stop();
        if (lyrics_) lyrics_->stop();
        RemoveClipboardFormatListener(window_);
        if (monitor_) monitor_->stop();
        if (powerNotification_) UnregisterPowerSettingNotification(powerNotification_);
        Shell_NotifyIconW(NIM_DELETE, &tray_);
        if (trayIcon_) DestroyIcon(trayIcon_);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window_, message, wparam, lparam);
}

}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int) {
    if (zenith::browserHostRequested()) return zenith::runBrowserHost(instance);
    const bool settingsRequested = std::wstring_view(commandLine) == L"--settings";
    std::optional<zenith::SettingsTest> test;
    auto feature = zenith::Feature::media;
    unsigned requested = 0, requestedFeature = 0;
    wchar_t extra = 0;
    if (swscanf_s(commandLine, L"--test=%u,%u%c", &requested, &requestedFeature, &extra, 1u) == 2 &&
        zenith::validSettingsTest(requested) && requestedFeature < zenith::featureOrder.size()) {
        test = static_cast<zenith::SettingsTest>(requested);
        feature = static_cast<zenith::Feature>(requestedFeature);
    }
    winrt::handle singleInstance;
    if (!settingsRequested) {
        const auto mutex = CreateMutexW(nullptr, FALSE, L"Local\\ZenithDot.Overlay.0A54ED53");
        const auto error = GetLastError();
        if (!mutex) return 1;
        singleInstance.attach(mutex);
        if (error == ERROR_ALREADY_EXISTS) {
            if (test) {
                if (const auto overlay = FindWindowW(zenith::overlayWindowClass, nullptr)) {
                    PostMessageW(overlay, zenith::settingsTestMessage, static_cast<WPARAM>(*test), static_cast<LPARAM>(feature));
                }
            }
            return 0;
        }
    }
    int result = 1;
    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        struct Ole {
            HRESULT status = OleInitialize(nullptr);
            ~Ole() { if (SUCCEEDED(status)) OleUninitialize(); }
        } ole;
        result = settingsRequested ? zenith::runSettingsWindow(instance) : zenith::App{}.run(instance, test, feature);
    } catch (winrt::hresult_error const& failure) {
        MessageBoxW(nullptr, failure.message().c_str(), L"Zenula could not start", MB_OK | MB_ICONERROR);
    }
    return result;
}
