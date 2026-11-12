#include "settings.hpp"
#include "browser_tabs.hpp"
#include "resource.h"
#include "notifications.hpp"
#include "prayer.hpp"

#include <commctrl.h>
#include <commdlg.h>
#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <winrt/base.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <span>

namespace zenith {
namespace {

constexpr wchar_t title[] = L"Zenith Dot Settings";
constexpr wchar_t repository[] = L"https://github.com/alifihsan7/Zenith-Dot";
constexpr COLORREF background = RGB(15, 18, 24);
constexpr COLORREF sidebar = RGB(20, 24, 32);
constexpr COLORREF surface = RGB(24, 29, 38);
constexpr COLORREF selected = RGB(31, 48, 69);
constexpr COLORREF foreground = RGB(240, 244, 250);
constexpr COLORREF muted = RGB(165, 178, 196);
constexpr COLORREF accent = RGB(145, 202, 255);
constexpr COLORREF border = RGB(47, 57, 72);
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
constexpr auto pageDuration = 180ms;
constexpr auto toggleDuration = 180ms;
constexpr UINT_PTR motionTimer = 1;

void notifyOverlay() {
    if (const auto overlay = FindWindowW(overlayWindowClass, nullptr)) {
        PostMessageW(overlay, settingsChangedMessage, 0, 0);
    }
}

struct Page {
    int id;
    wchar_t const* title;
    wchar_t const* description;
    wchar_t const* group;
};

constexpr Page pages[]{
    {IDC_SETTINGS_GENERAL, L"General", L"Choose how Zenith Dot starts on this PC.", L"Startup"},
    {IDC_SETTINGS_DISPLAY, L"Appearance", L"Adjust the island's size, shape, and accent.", nullptr},
    {IDC_SETTINGS_FEATURES, L"Features", L"Choose what appears in the island and arrange its feature bar.", nullptr},
    {IDC_SETTINGS_PRIVACY, L"Privacy & Clipboard", L"Choose which clipboard and device indicators appear.", nullptr},
    {IDC_SETTINGS_NOTIFICATIONS, L"Notifications", L"Manage island alerts and prayer reminders.", nullptr},
    {IDC_SETTINGS_ABOUT, L"About", L"Zenith Dot for Windows", nullptr},
};

struct Option {
    int id;
    int page;
    wchar_t const* description;
    bool Settings::*field;
    bool (Settings::*save)() const;
    int detail = 0;
};

constexpr Option options[]{
    {IDC_START_WITH_WINDOWS, IDC_SETTINGS_GENERAL, L"Launch Zenith Dot when you sign in.", nullptr, nullptr},
    {IDC_PRAYER_REMINDERS, IDC_SETTINGS_NOTIFICATIONS, L"Show a bubble when it’s time to pray.",
        &Settings::prayerReminders, &Settings::saveNotifications},
    {IDC_HIDE_FULLSCREEN, IDC_SETTINGS_DISPLAY, L"Hide the island over fullscreen apps and games.",
        &Settings::hideInFullscreen, &Settings::saveDisplay},
    {IDC_ADD_COPIED_TEXT, IDC_SETTINGS_FEATURES, L"Keep copied text in Drop Zone for this session.",
        &Settings::autoAddText, &Settings::saveClipboard, IDC_FEATURE_DROP},
    {IDC_ADD_SCREENSHOTS, IDC_SETTINGS_FEATURES, L"Add screenshots and other clipboard images to Drop Zone.",
        &Settings::autoAddScreenshots, &Settings::saveClipboard, IDC_FEATURE_DROP},
    {IDC_CLIPBOARD_BUBBLE, IDC_SETTINGS_PRIVACY, L"Show a notification when you copy something.",
        &Settings::showClipboardBubble, &Settings::saveClipboard},
    {IDC_CLIPBOARD_PREVIEWS, IDC_SETTINGS_PRIVACY, L"Include a short text snippet in the Copied bubble.",
        &Settings::showClipboardPreviews, &Settings::saveClipboard},
    {IDC_CAMERA_INDICATOR, IDC_SETTINGS_PRIVACY, L"Show when an app is using the camera.",
        &Settings::showCameraIndicator, &Settings::savePrivacy},
    {IDC_MICROPHONE_INDICATOR, IDC_SETTINGS_PRIVACY, L"Show when an app is using the microphone.",
        &Settings::showMicrophoneIndicator, &Settings::savePrivacy},
    {IDC_AUTO_CYCLE, IDC_SETTINGS_FEATURES, L"Start the next Focus or Break phase automatically.",
        &Settings::autoCycle, &Settings::saveTimer, IDC_FEATURE_TIMERS},
    {IDC_FEATURE_ENABLED, IDC_SETTINGS_FEATURES, L"Show this feature in the feature bar.", nullptr, nullptr},
    {IDC_SHOW_WAVEFORM, IDC_SETTINGS_FEATURES, L"Show the selected player's audio spectrum during playback.",
        &Settings::showWaveform, &Settings::saveIsland, IDC_FEATURE_MEDIA},
    {IDC_PILL_LYRICS, IDC_SETTINGS_FEATURES, L"Show the current line while the Lyrics view is active.",
        &Settings::showPillLyrics, &Settings::saveIsland, IDC_FEATURE_MEDIA},
    {IDC_ENABLE_MEDIA, IDC_SETTINGS_FEATURES, nullptr, nullptr, nullptr},
    {IDC_ENABLE_TIMERS, IDC_SETTINGS_FEATURES, nullptr, nullptr, nullptr},
    {IDC_ENABLE_SCRATCH, IDC_SETTINGS_FEATURES, nullptr, nullptr, nullptr},
    {IDC_ENABLE_DROP, IDC_SETTINGS_FEATURES, nullptr, nullptr, nullptr},
    {IDC_ENABLE_SYSTEM, IDC_SETTINGS_FEATURES, nullptr, nullptr, nullptr},
};

bool isFeatureSwitch(int id) { return id >= IDC_ENABLE_MEDIA && id <= IDC_ENABLE_SYSTEM; }
Feature featureForSwitch(int id) { return static_cast<Feature>(id - IDC_ENABLE_MEDIA); }
bool hovered(HWND window) {
    POINT pointer{};
    return IsWindowEnabled(window) && GetCursorPos(&pointer) && WindowFromPoint(pointer) == window;
}

constexpr wchar_t const* featureNames[]{L"Now Playing", L"Timers", L"Scratchpad", L"Drop Zone", L"System"};
constexpr wchar_t const* widthChoices[]{L"Fit to title", L"147 px minimum"};
constexpr wchar_t const* sizeChoices[]{L"Compact", L"Default", L"Large"};
constexpr wchar_t const* accentChoices[]{L"Album artwork", L"Zenith blue", L"Custom"};
constexpr wchar_t const* pauseChoices[]{L"Immediately", L"3 seconds", L"5 seconds", L"Keep the title"};
constexpr wchar_t const* focusChoices[]{L"15 minutes", L"25 minutes", L"45 minutes"};
constexpr wchar_t const* breakChoices[]{L"5 minutes", L"10 minutes", L"15 minutes"};
constexpr wchar_t const* quietChoices[]{L"Off", L"Follow Windows", L"On"};
constexpr wchar_t const* prayerChoices[]{
    L"Off", prayerLocations[0].name, prayerLocations[1].name, prayerLocations[2].name
};
constexpr int choiceControls[]{IDC_PILL_WIDTH, IDC_ISLAND_SIZE, IDC_ACCENT_MODE, IDC_CUSTOM_ACCENT,
    IDC_PAUSE_DELAY, IDC_FOCUS_DURATION, IDC_BREAK_DURATION, IDC_QUIET_MODE, IDC_PRAYER_CITY};

struct TestButton {
    int control;
    SettingsTest test;
};

constexpr int firstTestButton = 1200;
constexpr int firstChoiceButton = 1300;
constexpr int choicesPerRow = 4;

int choiceButtonId(size_t row, size_t option) {
    return firstChoiceButton + static_cast<int>(row * choicesPerRow + option);
}

bool isChoiceButton(int id) {
    return id >= firstChoiceButton && id < choiceButtonId(std::size(choiceControls), 0);
}

int choiceRowId(int id) {
    return choiceControls[(id - firstChoiceButton) / choicesPerRow];
}

unsigned choiceIndex(int id) {
    return static_cast<unsigned>((id - firstChoiceButton) % choicesPerRow);
}
constexpr TestButton testButtons[]{
    {IDC_PILL_WIDTH, SettingsTest::appearance},
    {IDC_SHOW_WAVEFORM, SettingsTest::waveform},
    {IDC_PILL_LYRICS, SettingsTest::lyrics},
    {IDC_PAUSE_DELAY, SettingsTest::pause},
    {IDC_FOCUS_DURATION, SettingsTest::focus},
    {IDC_BREAK_DURATION, SettingsTest::resting},
    {IDC_AUTO_CYCLE, SettingsTest::cycle},
    {IDC_ADD_COPIED_TEXT, SettingsTest::copiedText},
    {IDC_ADD_SCREENSHOTS, SettingsTest::screenshot},
    {IDC_CLIPBOARD_BUBBLE, SettingsTest::copied},
    {IDC_CAMERA_INDICATOR, SettingsTest::camera},
    {IDC_MICROPHONE_INDICATOR, SettingsTest::microphone},
    {IDC_FEATURE_DOWN, SettingsTest::order},
};

bool isTestButton(int id) {
    return id >= firstTestButton && id < firstTestButton + static_cast<int>(std::size(testButtons));
}

std::span<wchar_t const* const> choices(int id) {
    switch (id) {
    case IDC_QUIET_MODE: return quietChoices;
    case IDC_PRAYER_CITY: return prayerChoices;
    case IDC_PILL_WIDTH: return widthChoices;
    case IDC_ISLAND_SIZE: return sizeChoices;
    case IDC_ACCENT_MODE: return accentChoices;
    case IDC_PAUSE_DELAY: return pauseChoices;
    case IDC_FOCUS_DURATION: return focusChoices;
    case IDC_BREAK_DURATION: return breakChoices;
    default: return {};
    }
}

D2D1_RECT_F rect(RECT value) {
    return D2D1::RectF(static_cast<float>(value.left), static_cast<float>(value.top),
                       static_cast<float>(value.right), static_cast<float>(value.bottom));
}

D2D1_COLOR_F color(COLORREF value) {
    return D2D1::ColorF(GetRValue(value) / 255.0f, GetGValue(value) / 255.0f, GetBValue(value) / 255.0f);
}

D2D1_COLOR_F blend(COLORREF from, COLORREF to, float amount) {
    const auto a = color(from);
    const auto b = color(to);
    return D2D1::ColorF(std::lerp(a.r, b.r, amount), std::lerp(a.g, b.g, amount), std::lerp(a.b, b.b, amount));
}

float progress(Clock::time_point start, Clock::duration duration, Clock::time_point now) {
    if (start == Clock::time_point{} || now - start >= duration) return 1;
    const float t = std::chrono::duration<float>(now - start) / std::chrono::duration<float>(duration);
    const float remaining = 1 - t;
    return 1 - remaining * remaining * remaining;
}

struct ToggleMotion {
    float from = 0;
    float to = 0;
    Clock::time_point start{};

    float value(Clock::time_point now) const {
        return std::lerp(from, to, progress(start, toggleDuration, now));
    }
};

class Canvas {
public:
    Canvas();
    void setDpi(UINT dpi);
    bool begin(HDC dc, RECT area, COLORREF clear);
    void end();
    void fill(RECT area, COLORREF value);
    void rounded(D2D1_RECT_F area, float radius, D2D1_COLOR_F value);
    void focus(RECT area, float radius, COLORREF value);
    void text(RECT area, wchar_t const* value, IDWriteTextFormat* font, D2D1_COLOR_F ink);
    IDWriteTextFormat* body() const { return body_.get(); }
    IDWriteTextFormat* strong() const { return strong_.get(); }
    IDWriteTextFormat* heading() const { return heading_.get(); }

private:
    winrt::com_ptr<ID2D1Factory> graphics_;
    winrt::com_ptr<IDWriteFactory> writing_;
    winrt::com_ptr<ID2D1DCRenderTarget> target_;
    winrt::com_ptr<ID2D1SolidColorBrush> brush_;
    winrt::com_ptr<IDWriteTextFormat> body_, strong_, heading_;
};

Canvas::Canvas() {
    winrt::check_hresult(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, graphics_.put()));
    winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                             reinterpret_cast<IUnknown**>(writing_.put())));
    setDpi(96);
}

void Canvas::setDpi(UINT dpi) {
    const auto create = [&](winrt::com_ptr<IDWriteTextFormat>& font, float size, DWRITE_FONT_WEIGHT weight) {
        winrt::com_ptr<IDWriteTextFormat> next;
        if (SUCCEEDED(writing_->CreateTextFormat(L"Segoe UI", nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, size * dpi / 96.0f, L"", next.put()))) {
            next->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            next->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            font = std::move(next);
        }
    };
    create(body_, 13, DWRITE_FONT_WEIGHT_NORMAL);
    create(strong_, 14, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    create(heading_, 30, DWRITE_FONT_WEIGHT_SEMI_BOLD);
}

bool Canvas::begin(HDC dc, RECT area, COLORREF clear) {
    if (!target_) {
        const auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), 96, 96);
        if (FAILED(graphics_->CreateDCRenderTarget(&properties, target_.put()))) return false;
        if (FAILED(target_->CreateSolidColorBrush(color(clear), brush_.put()))) {
            target_ = nullptr;
            return false;
        }
        target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    }
    if (FAILED(target_->BindDC(dc, &area))) return false;
    target_->BeginDraw();
    target_->Clear(color(clear));
    return true;
}

void Canvas::end() {
    if (FAILED(target_->EndDraw())) {
        brush_ = nullptr;
        target_ = nullptr;
    }
}

void Canvas::fill(RECT area, COLORREF value) {
    brush_->SetColor(color(value));
    target_->FillRectangle(rect(area), brush_.get());
}

void Canvas::rounded(D2D1_RECT_F area, float radius, D2D1_COLOR_F value) {
    brush_->SetColor(value);
    target_->FillRoundedRectangle(D2D1::RoundedRect(area, radius, radius), brush_.get());
}

void Canvas::focus(RECT area, float radius, COLORREF value) {
    brush_->SetColor(color(value));
    target_->DrawRoundedRectangle(D2D1::RoundedRect(rect(area), radius, radius), brush_.get());
}

void Canvas::text(RECT area, wchar_t const* value, IDWriteTextFormat* font, D2D1_COLOR_F ink) {
    if (!font) return;
    brush_->SetColor(ink);
    target_->DrawText(value, static_cast<UINT32>(wcslen(value)), font, rect(area), brush_.get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

class SettingsWindow {
public:
    static INT_PTR CALLBACK dialog(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

private:
    static LRESULT CALLBACK optionProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                                      UINT_PTR id, DWORD_PTR data);
    void initialize(HWND window);
    void setDpi(UINT dpi);
    void selectPage(int id);
    void sync();
    bool command(int id);
    bool commandChoice(int id);
    void runTest(SettingsTest test);
    void completeNotificationAccess(NotificationAccess access);
    void showControls();
    void openDetail(int id);
    unsigned currentChoice(int id) const;
    Feature feature() const { return static_cast<Feature>(detail_ - IDC_FEATURE_MEDIA); }
    void redrawMotion();
    float pageOpacity() const;
    void paint(HDC dc);
    void drawOption(HWND control, HDC dc, Option const& option);
    void drawButton(DRAWITEMSTRUCT const& item);
    RECT units(int left, int top, int right, int bottom) const {
        RECT rect{left, top, right, bottom};
        MapDialogRect(window_, &rect);
        return rect;
    }
    int px(int value) const { return MulDiv(value, dpi_, 96); }

    HWND window_{};
    HICON icon_{};
    Canvas canvas_;
    UINT dpi_ = 96;
    int page_ = IDC_SETTINGS_GENERAL;
    int detail_ = 0;
    Settings settings_;
    bool animate_ = true;
    Clock::time_point pageStart_{};
    std::array<ToggleMotion, std::size(options)> toggles_{};
    std::array<ToggleMotion, std::size(choiceControls) * choicesPerRow> choiceMotion_{};
    std::wstring startup_;
    winrt::Windows::Foundation::IAsyncAction notificationRequest_{nullptr};
    bool packaged_ = false;
};

void SettingsWindow::setDpi(UINT dpi) {
    dpi_ = dpi;
    canvas_.setDpi(dpi);
}

void SettingsWindow::initialize(HWND window) {
    window_ = window;
    packaged_ = hasPackageIdentity();
    setDpi(GetDpiForWindow(window));
    BOOL motion = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &motion, 0);
    animate_ = motion != FALSE;
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    icon_ = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_ZENITH_DOT));
    if (icon_) SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon_));
    startup_ = startupShortcut();
    EnableWindow(GetDlgItem(window, IDC_START_WITH_WINDOWS), !startup_.empty());
    for (const auto& option : options) {
        SetWindowSubclass(GetDlgItem(window, option.id), optionProc, 0, reinterpret_cast<DWORD_PTR>(this));
    }
    for (const auto& page : pages)
        SetWindowSubclass(GetDlgItem(window, page.id), optionProc, 0, reinterpret_cast<DWORD_PTR>(this));
    for (const int id : choiceControls)
        SetWindowSubclass(GetDlgItem(window, id), optionProc, 0, reinterpret_cast<DWORD_PTR>(this));
    for (const int id : {IDC_APP_NOTIFICATIONS, IDC_TEST_NOTIFICATION, IDC_SETTINGS_GITHUB, IDC_FEATURE_BACK, IDC_FEATURE_UP, IDC_FEATURE_DOWN,
                        IDC_FEATURE_MEDIA, IDC_FEATURE_TIMERS, IDC_FEATURE_SCRATCH, IDC_FEATURE_DROP, IDC_FEATURE_SYSTEM}) {
        SetWindowSubclass(GetDlgItem(window, id), optionProc, 0, reinterpret_cast<DWORD_PTR>(this));
    }
    const auto instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(window, GWLP_HINSTANCE));
    for (size_t row = 0; row < std::size(choiceControls); ++row) {
        const int id = choiceControls[row];
        const auto values = choices(id);
        if (values.empty()) continue;
        const auto heading = GetDlgItem(window, id);
        EnableWindow(heading, FALSE);
        auto previous = heading;
        for (size_t i = 0; i < values.size(); ++i) {
            const auto button = CreateWindowExW(0, L"Button", values[i], WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(choiceButtonId(row, i))),
                instance, nullptr);
            if (!button) continue;
            SetWindowSubclass(button, optionProc, 0, reinterpret_cast<DWORD_PTR>(this));
            SetWindowPos(button, previous, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            previous = button;
        }
    }
    for (size_t i = 0; i < std::size(testButtons); ++i) {
        wchar_t label[96]{};
        GetWindowTextW(GetDlgItem(window, testButtons[i].control), label, ARRAYSIZE(label));
        const auto test = testButtons[i].test;
        const auto name = test == SettingsTest::appearance ? L"Test appearance" :
            test == SettingsTest::order ? L"Test feature order" :
            test == SettingsTest::copied ? L"Test clipboard" : L"Test " + std::wstring(label);
        const auto id = firstTestButton + static_cast<int>(i);
        const auto button = CreateWindowExW(0, L"Button", name.c_str(), WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
        if (button) SetWindowSubclass(button, optionProc, 0, reinterpret_cast<DWORD_PTR>(this));
    }
    sync();
}

void SettingsWindow::selectPage(int id) {
    if (page_ == id && !detail_) return;
    page_ = id;
    openDetail(0);
}

void SettingsWindow::openDetail(int id) {
    detail_ = id;
    if (animate_) {
        pageStart_ = Clock::now();
        SetTimer(window_, motionTimer, 16, nullptr);
    }
    sync();
}

void SettingsWindow::showControls() {
    const bool listing = page_ == IDC_SETTINGS_FEATURES && !detail_;
    const bool appearance = page_ == IDC_SETTINGS_DISPLAY;
    const bool enabled = detail_ && settings_.enabled(feature());
    const auto show = [this](int id, bool visible) { ShowWindow(GetDlgItem(window_, id), visible ? SW_SHOW : SW_HIDE); };
    for (const auto& option : options) {
        bool visible = option.page == page_ && (option.detail == 0 || (option.detail == detail_ && enabled));
        if (option.id == IDC_FEATURE_ENABLED) visible = detail_ != 0;
        if (isFeatureSwitch(option.id)) visible = listing;
        show(option.id, visible);
    }
    for (size_t row = 0; row < std::size(choiceControls); ++row) {
        const int id = choiceControls[row];
        const bool visible = (id == IDC_QUIET_MODE || id == IDC_PRAYER_CITY) ? page_ == IDC_SETTINGS_NOTIFICATIONS :
            id == IDC_PAUSE_DELAY ? detail_ == IDC_FEATURE_MEDIA && enabled :
            id == IDC_FOCUS_DURATION || id == IDC_BREAK_DURATION ? detail_ == IDC_FEATURE_TIMERS && enabled :
            appearance && (id != IDC_CUSTOM_ACCENT || settings_.accentMode == 2);
        show(id, visible);
        const auto values = choices(id);
        if (!visible || values.empty()) {
            for (size_t i = 0; i < values.size(); ++i) show(choiceButtonId(row, i), false);
            continue;
        }
        RECT heading{};
        GetWindowRect(GetDlgItem(window_, id), &heading);
        MapWindowPoints(nullptr, window_, reinterpret_cast<POINT*>(&heading), 2);
        const auto metrics = units(0, 0, 312, 39);
        const auto inset = units(0, 0, 5, 16);
        SetWindowPos(GetDlgItem(window_, id), nullptr, heading.left, heading.top,
                     metrics.right, inset.bottom, SWP_NOZORDER | SWP_NOACTIVATE);
        const auto top = heading.top + inset.bottom;
        const auto height = metrics.bottom - inset.bottom - px(4);
        const auto available = (id == IDC_PAUSE_DELAY ||
            id == IDC_FOCUS_DURATION || id == IDC_BREAK_DURATION) ? units(0, 0, 258, 0).right : metrics.right;
        const auto width = available / static_cast<int>(values.size());
        for (size_t i = 0; i < values.size(); ++i) {
            const auto button = GetDlgItem(window_, choiceButtonId(row, i));
            const auto left = heading.left + static_cast<int>(i) * width;
            const bool chosen = i == currentChoice(id);
            const auto label = chosen ? std::wstring(values[i]) + L", selected" : std::wstring(values[i]);
            SetWindowTextW(button, label.c_str());
            SetWindowPos(button, nullptr, left, top, width - inset.right, height,
                         SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
    }
    const int fullscreenTop = settings_.accentMode == 2 ? 220 : 190;
    const auto fullscreen = units(144, fullscreenTop, 456, fullscreenTop + 30);
    SetWindowPos(GetDlgItem(window_, IDC_HIDE_FULLSCREEN), nullptr, fullscreen.left, fullscreen.top,
                 fullscreen.right - fullscreen.left, fullscreen.bottom - fullscreen.top, SWP_NOZORDER | SWP_NOACTIVATE);
    for (size_t index = 0; index < settings_.features.size(); ++index) {
        const auto id = IDC_FEATURE_MEDIA + static_cast<int>(settings_.features[index]);
        const int y = 69 + 35 * static_cast<int>(index);
        const auto bounds = units(144, y, 413, y + 30);
        SetWindowPos(GetDlgItem(window_, id), nullptr, bounds.left, bounds.top, bounds.right - bounds.left,
                     bounds.bottom - bounds.top, SWP_NOZORDER | SWP_NOACTIVATE);
        const auto toggle = units(415, y, 456, y + 30);
        const int toggleId = IDC_ENABLE_MEDIA + static_cast<int>(settings_.features[index]);
        SetWindowPos(GetDlgItem(window_, toggleId), GetDlgItem(window_, id), toggle.left, toggle.top,
                     toggle.right - toggle.left, toggle.bottom - toggle.top, SWP_NOACTIVATE);
        show(id, listing);
    }
    show(IDC_FEATURE_BACK, detail_ != 0);
    show(IDC_FEATURE_UP, detail_ != 0);
    show(IDC_FEATURE_DOWN, detail_ != 0);
    if (detail_) {
        const auto where = std::ranges::find(settings_.features, feature());
        EnableWindow(GetDlgItem(window_, IDC_FEATURE_UP), where != settings_.features.begin());
        EnableWindow(GetDlgItem(window_, IDC_FEATURE_DOWN), where + 1 != settings_.features.end());
    }
    const auto appNotifications = GetDlgItem(window_, IDC_APP_NOTIFICATIONS);
    SetWindowTextW(appNotifications, settings_.showAppNotifications ? L"Turn off Windows app alerts" : L"Connect Windows apps");
    EnableWindow(appNotifications, packaged_ && !notificationRequest_);
    show(IDC_APP_NOTIFICATIONS, page_ == IDC_SETTINGS_NOTIFICATIONS && packaged_);
    show(IDC_TEST_NOTIFICATION, page_ == IDC_SETTINGS_NOTIFICATIONS);
    show(IDC_SETTINGS_GITHUB, page_ == IDC_SETTINGS_ABOUT);
    EnableWindow(GetDlgItem(window_, IDC_PRAYER_REMINDERS), settings_.prayerCity != 0);
    EnableWindow(GetDlgItem(window_, IDC_CLIPBOARD_PREVIEWS), settings_.showClipboardBubble);
    const auto column = units(410, 0, 456, 0);
    for (size_t i = 0; i < std::size(testButtons); ++i) {
        const auto control = GetDlgItem(window_, testButtons[i].control);
        const auto button = GetDlgItem(window_, firstTestButton + static_cast<int>(i));
        const bool visible = (GetWindowLongPtrW(control, GWL_STYLE) & WS_VISIBLE) != 0;
        if (testButtons[i].test == SettingsTest::appearance) {
            const auto test = units(374, 13, 456, 34);
            SetWindowPos(button, nullptr, test.left, test.top, test.right - test.left, test.bottom - test.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        } else if (visible) {
            RECT row{};
            GetWindowRect(control, &row);
            MapWindowPoints(nullptr, window_, reinterpret_cast<POINT*>(&row), 2);
            SetWindowPos(control, nullptr, row.left, row.top, column.left - px(8) - row.left, row.bottom - row.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            const bool choice = !choices(testButtons[i].control).empty();
            const auto height = choice ? px(30) : std::min<LONG>(row.bottom - row.top, px(33));
            const auto top = choice ? row.bottom + px(1) : (row.top + row.bottom - height) / 2;
            SetWindowPos(button, control, column.left, top,
                         column.right - column.left, height, SWP_NOACTIVATE);
        }
        ShowWindow(button, visible ? SW_SHOW : SW_HIDE);
    }
    RedrawWindow(window_, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void SettingsWindow::runTest(SettingsTest test) {
    const auto target = detail_ ? feature() : Feature::media;
    if (const auto overlay = FindWindowW(overlayWindowClass, nullptr)) {
        DWORD_PTR accepted = 0;
        if (SendMessageTimeoutW(overlay, settingsTestMessage, static_cast<WPARAM>(test), static_cast<LPARAM>(target),
                                SMTO_ABORTIFHUNG, 1500, &accepted) && accepted) return;
        MessageBoxW(window_, L"Restart Zenith Dot with this build to run the test.", title, MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring executable(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    const auto args = L"--test=" + std::to_wstring(static_cast<unsigned>(test)) + L"," +
        std::to_wstring(static_cast<unsigned>(target));
    if (!length || length >= executable.size() ||
        reinterpret_cast<INT_PTR>(ShellExecuteW(window_, L"open", executable.c_str(), args.c_str(),
                                               nullptr, SW_SHOWNOACTIVATE)) <= 32) {
        MessageBoxW(window_, L"Couldn’t start Zenith Dot to run the test.", title, MB_OK | MB_ICONWARNING);
    }
}

void SettingsWindow::sync() {
    settings_ = Settings::load();
    for (size_t row = 0; row < std::size(choiceControls); ++row) {
        const auto id = choiceControls[row];
        for (size_t i = 0; i < choices(id).size(); ++i) {
            const float value = i == currentChoice(id) ? 1.0f : 0.0f;
            auto& motion = choiceMotion_[choiceButtonId(row, i) - firstChoiceButton];
            if (motion.to != value) motion = {value, value, {}};
        }
    }
    for (const auto& option : options) {
        const bool checked = isFeatureSwitch(option.id) ? settings_.enabled(featureForSwitch(option.id)) : option.field ? settings_.*option.field :
            option.id == IDC_FEATURE_ENABLED ? detail_ && settings_.enabled(feature()) : startupEnabled(startup_);
        auto& motion = toggles_[&option - options];
        if (motion.to != static_cast<float>(checked)) motion = {static_cast<float>(checked), static_cast<float>(checked), {}};
        CheckDlgButton(window_, option.id, checked ? BST_CHECKED : BST_UNCHECKED);
        InvalidateRect(GetDlgItem(window_, option.id), nullptr, FALSE);
    }
    showControls();
}

unsigned SettingsWindow::currentChoice(int id) const {
    switch (id) {
    case IDC_QUIET_MODE: return static_cast<unsigned>(settings_.quietMode);
    case IDC_PRAYER_CITY: return settings_.prayerCity;
    case IDC_PILL_WIDTH: return settings_.fixedPillMinimum;
    case IDC_ISLAND_SIZE: return settings_.islandSize;
    case IDC_ACCENT_MODE: return settings_.accentMode;
    case IDC_PAUSE_DELAY: return settings_.compactPause;
    case IDC_FOCUS_DURATION: return settings_.focusPreset;
    case IDC_BREAK_DURATION: return settings_.breakPreset;
    default: return 0;
    }
}

bool SettingsWindow::commandChoice(int id) {
    if (id == IDC_CUSTOM_ACCENT) {
        COLORREF swatches[16]{};
        CHOOSECOLORW picker{sizeof(picker)};
        picker.hwndOwner = window_;
        picker.rgbResult = RGB(settings_.customAccent >> 16, settings_.customAccent >> 8, settings_.customAccent);
        picker.lpCustColors = swatches;
        picker.Flags = CC_FULLOPEN | CC_RGBINIT;
        if (!ChooseColorW(&picker)) return true;
        auto updated = Settings::load();
        updated.customAccent = (GetRValue(picker.rgbResult) << 16) | (GetGValue(picker.rgbResult) << 8) |
            GetBValue(picker.rgbResult);
        if (updated.saveAccent()) notifyOverlay();
        else MessageBoxW(window_, L"Couldn’t save this color.", title, MB_OK | MB_ICONWARNING);
        sync();
        return true;
    }
    if (!isChoiceButton(id)) return false;
    const auto index = choiceIndex(id);
    id = choiceRowId(id);
    if (index >= choices(id).size() || index == currentChoice(id)) return true;
    auto updated = Settings::load();
    switch (id) {
    case IDC_QUIET_MODE: updated.quietMode = static_cast<QuietMode>(index); break;
    case IDC_PRAYER_CITY: updated.prayerCity = index; break;
    case IDC_PILL_WIDTH: updated.fixedPillMinimum = index != 0; break;
    case IDC_ISLAND_SIZE: updated.islandSize = index; break;
    case IDC_ACCENT_MODE: updated.accentMode = index; break;
    case IDC_PAUSE_DELAY: updated.compactPause = index; break;
    case IDC_FOCUS_DURATION: updated.focusPreset = index; break;
    case IDC_BREAK_DURATION: updated.breakPreset = index; break;
    }
    const bool saved = id == IDC_QUIET_MODE || id == IDC_PRAYER_CITY
        ? updated.saveNotifications() : updated.saveIsland();
    if (saved) {
        const auto before = currentChoice(id);
        settings_ = updated;
        const auto base = (std::ranges::find(choiceControls, id) - std::begin(choiceControls)) * choicesPerRow;
        const auto now = Clock::now();
        for (const auto changed : {before, index}) {
            auto& motion = choiceMotion_[base + changed];
            const float destination = changed == index ? 1.0f : 0.0f;
            motion = {animate_ ? motion.value(now) : destination, destination, animate_ ? now : Clock::time_point{}};
        }
        if (animate_) SetTimer(window_, motionTimer, 16, nullptr);
        showControls();
        notifyOverlay();
    } else MessageBoxW(window_, L"Couldn’t save this setting.", title, MB_OK | MB_ICONWARNING);
    return true;
}

void SettingsWindow::completeNotificationAccess(NotificationAccess access) {
    notificationRequest_ = nullptr;
    if (access != NotificationAccess::Allowed) {
        showControls();
        MessageBoxW(window_, L"Allow Zenith Dot in Windows Settings under Privacy & security > Notifications.",
                    title, MB_OK | MB_ICONINFORMATION);
        return;
    }
    auto updated = Settings::load();
    updated.showAppNotifications = true;
    if (updated.saveNotifications()) notifyOverlay();
    else MessageBoxW(window_, L"Couldn’t save this setting.", title, MB_OK | MB_ICONWARNING);
    sync();
}

bool SettingsWindow::command(int id) {
    if (id == IDC_TEST_NOTIFICATION) {
        runTest(SettingsTest::notification);
        return true;
    }
    if (id == IDC_APP_NOTIFICATIONS && packaged_ && !notificationRequest_) {
        if (!settings_.showAppNotifications) {
            notificationRequest_ = requestNotificationAccess(window_);
            showControls();
        } else {
            auto updated = Settings::load();
            updated.showAppNotifications = false;
            if (updated.saveNotifications()) notifyOverlay();
            else MessageBoxW(window_, L"Couldn’t save this setting.", title, MB_OK | MB_ICONWARNING);
            sync();
        }
        return true;
    }
    if (isTestButton(id)) {
        runTest(testButtons[id - firstTestButton].test);
        return true;
    }
    for (const auto& page : pages) {
        if (id == page.id) {
            selectPage(id);
            return true;
        }
    }
    if (id >= IDC_FEATURE_MEDIA && id <= IDC_FEATURE_SYSTEM) {
        openDetail(id);
        SetFocus(GetDlgItem(window_, IDC_FEATURE_BACK));
        return true;
    }
    if (id == IDC_FEATURE_BACK) {
        const auto previous = detail_;
        openDetail(0);
        SetFocus(GetDlgItem(window_, previous));
        return true;
    }
    if (detail_ && (id == IDC_FEATURE_UP || id == IDC_FEATURE_DOWN)) {
        auto updated = Settings::load();
        const auto current = std::ranges::find(updated.features, feature()) - updated.features.begin();
        const auto next = current + (id == IDC_FEATURE_UP ? -1 : 1);
        if (next >= 0 && next < static_cast<ptrdiff_t>(updated.features.size())) {
            std::swap(updated.features[current], updated.features[next]);
            if (updated.saveFeatures()) notifyOverlay();
            else MessageBoxW(window_, L"Couldn’t save the feature order.", title, MB_OK | MB_ICONWARNING);
            sync();
        }
        return true;
    }
    if (commandChoice(id)) return true;
    if (id == IDC_SETTINGS_GITHUB) {
        ShellExecuteW(window_, L"open", repository, nullptr, nullptr, SW_SHOWNORMAL);
        return true;
    }
    for (const auto& option : options) {
        if (id != option.id) continue;
        const bool checked = IsDlgButtonChecked(window_, id) == BST_CHECKED;
        bool saved;
        if (option.field) {
            auto updated = Settings::load();
            updated.*option.field = checked;
            saved = (updated.*option.save)();
            if (saved) { settings_ = updated; notifyOverlay(); }
        } else if (id == IDC_FEATURE_ENABLED || isFeatureSwitch(id)) {
            auto updated = Settings::load();
            const auto bit = 1u << static_cast<unsigned>(isFeatureSwitch(id) ? featureForSwitch(id) : feature());
            if (checked) updated.enabledFeatures |= bit;
            else updated.enabledFeatures &= ~bit;
            saved = updated.saveFeatures();
            if (saved) { settings_ = updated; notifyOverlay(); }
        } else {
            saved = setStartupEnabled(startup_, checked);
        }
        if (!saved) {
            sync();
            MessageBoxW(window_, L"Couldn’t save this setting.", title, MB_OK | MB_ICONWARNING);
        } else {
            const auto now = Clock::now();
            auto& motion = toggles_[&option - options];
            const float destination = checked ? 1.0f : 0.0f;
            motion = {animate_ ? motion.value(now) : destination, destination, animate_ ? now : Clock::time_point{}};
            if (animate_) SetTimer(window_, motionTimer, 16, nullptr);
            showControls();
        }
        return true;
    }
    return false;
}

float SettingsWindow::pageOpacity() const {
    return animate_ ? progress(pageStart_, pageDuration, Clock::now()) : 1.0f;
}

void SettingsWindow::redrawMotion() {
    const auto now = Clock::now();
    const bool pageMoving = animate_ && now - pageStart_ < pageDuration;
    bool moving = pageMoving;
    if (pageStart_ != Clock::time_point{}) {
        auto content = units(122, 0, 478, 288);
        InvalidateRect(window_, &content, FALSE);
        for (size_t row = 0; row < std::size(choiceControls); ++row) {
            const auto id = choiceControls[row];
            InvalidateRect(GetDlgItem(window_, id), nullptr, FALSE);
            for (size_t i = 0; i < choices(id).size(); ++i)
                InvalidateRect(GetDlgItem(window_, choiceButtonId(row, i)), nullptr, FALSE);
        }
        for (int id = IDC_FEATURE_MEDIA; id <= IDC_FEATURE_SYSTEM; ++id)
            InvalidateRect(GetDlgItem(window_, id), nullptr, FALSE);
        if (page_ == IDC_SETTINGS_ABOUT) InvalidateRect(GetDlgItem(window_, IDC_SETTINGS_GITHUB), nullptr, FALSE);
    }
    for (size_t i = 0; i < choiceMotion_.size(); ++i) {
        auto& motion = choiceMotion_[i];
        if (motion.start == Clock::time_point{}) continue;
        InvalidateRect(GetDlgItem(window_, firstChoiceButton + static_cast<int>(i)), nullptr, FALSE);
        if (animate_ && now - motion.start < toggleDuration) moving = true;
        else motion.start = {};
    }
    for (const auto& option : options) {
        auto& motion = toggles_[&option - options];
        if (option.page == page_ && (pageStart_ != Clock::time_point{} || motion.start != Clock::time_point{})) {
            InvalidateRect(GetDlgItem(window_, option.id), nullptr, FALSE);
        }
        if (animate_ && now - motion.start < toggleDuration) moving = true;
        else motion.start = {};
    }
    if (!pageMoving) pageStart_ = {};
    if (!moving) KillTimer(window_, motionTimer);
}

void SettingsWindow::paint(HDC dc) {
    RECT client{};
    GetClientRect(window_, &client);
    if (!canvas_.begin(dc, client, background)) return;
    const auto rail = units(0, 0, 122, 0);
    canvas_.fill({0, 0, rail.right, client.bottom}, sidebar);
    canvas_.fill({rail.right - 1, 0, rail.right, client.bottom}, border);
    canvas_.text(units(38, 12, 115, 34), L"Zenith Dot", canvas_.strong(), color(foreground));

    const float phase = pageOpacity();
    const auto label = [&](RECT area, wchar_t const* value, IDWriteTextFormat* font, COLORREF ink) {
        const int offset = static_cast<int>(std::round(px(5) * (1 - phase)));
        OffsetRect(&area, 0, offset);
        canvas_.text(area, value, font, blend(background, ink, std::lerp(0.72f, 1.0f, phase)));
    };
    const auto& page = *std::ranges::find(pages, page_, &Page::id);
    if (detail_) {
        label(units(144, 31, 456, 57), featureNames[static_cast<unsigned>(feature())], canvas_.heading(), foreground);
        const auto position = std::ranges::find(settings_.features, feature()) - settings_.features.begin();
        const auto order = L"Feature bar position  " + std::to_wstring(position + 1) + L" of " +
            std::to_wstring(settings_.features.size());
        label(units(145, 211, 276, 234), order.c_str(), canvas_.body(), muted);
    } else {
        label(units(144, 12, 456, 38), page.title, canvas_.heading(), foreground);
        label(units(145, 39, 456, 53), page.description, canvas_.body(), muted);
        if (page.group) label(units(145, 61, 456, 75), page.group, canvas_.strong(), foreground);
    }
    if (page_ == IDC_SETTINGS_NOTIFICATIONS) {
        label(units(145, 211, 456, 226),
              packaged_ ? L"Windows apps · Show the app name and notification title."
                        : L"Windows app alerts require the MSIX build.",
              canvas_.body(), muted);
        label(units(145, 108, 456, 122), L"Silences island alerts. Camera and mic status remain visible.",
              canvas_.body(), muted);
    }
    if (page_ == IDC_SETTINGS_ABOUT) {
        label(units(145, 70, 418, 92), L"Version " ZENITH_VERSION_TEXT, canvas_.strong(), foreground);
    }
    canvas_.end();
    const auto logo = units(13, 13, 13, 13);
    if (icon_) DrawIconEx(dc, logo.left, logo.top, icon_, px(32), px(32), 0, nullptr, DI_NORMAL);
}

void SettingsWindow::drawOption(HWND control, HDC dc, Option const& option) {
    RECT row{};
    GetClientRect(control, &row);
    if (!canvas_.begin(dc, row, background)) return;
    const float phase = std::lerp(0.72f, 1.0f, pageOpacity());
    canvas_.rounded(rect(row), static_cast<float>(px(8)), blend(background, hovered(control) ? selected : surface, phase));
    const bool enabled = IsWindowEnabled(control) != FALSE;
    const float checked = animate_ ? toggles_[&option - options].value(Clock::now()) : toggles_[&option - options].to;
    const bool listToggle = isFeatureSwitch(option.id);
    const int right = listToggle ? (row.right + px(34)) / 2 : row.right - px(15);
    const int middle = row.bottom / 2;
    const RECT track{right - px(34), middle - px(10), right, middle + px(10)};
    const float active = enabled ? checked : 0;
    canvas_.rounded(rect(track), static_cast<float>(px(10)), blend(border, accent, active * phase));
    const float knob = std::lerp(static_cast<float>(track.left + px(3)), static_cast<float>(track.right - px(17)), checked);
    const float radius = static_cast<float>(px(7));
    canvas_.rounded(D2D1::RectF(knob, middle - radius, knob + 2 * radius, middle + radius),
                    radius, blend(muted, selected, active));

    wchar_t label[96]{};
    GetWindowTextW(control, label, ARRAYSIZE(label));
    const int left = px(14);
    const int textRight = track.left - px(12);
    if (!listToggle) {
        canvas_.text({left, px(5), textRight, middle + px(1)}, label, canvas_.strong(),
                     blend(surface, enabled ? foreground : muted, phase));
        const auto* description = option.id == IDC_START_WITH_WINDOWS
            ? (toggles_[&option - options].to > 0 ? L"Enabled · Launch at sign-in." : L"Disabled · Launch manually.")
            : option.description;
        canvas_.text({left, middle, textRight, row.bottom - px(6)}, description, canvas_.body(),
                     blend(surface, muted, phase));
    }
    if (GetFocus() == control) {
        InflateRect(&row, -px(2), -px(2));
        canvas_.focus(row, static_cast<float>(px(7)), accent);
    }
    canvas_.end();
}

void SettingsWindow::drawButton(DRAWITEMSTRUCT const& item) {
    RECT area{0, 0, item.rcItem.right - item.rcItem.left, item.rcItem.bottom - item.rcItem.top};
    const int id = static_cast<int>(item.CtlID);
    const auto page = std::ranges::find(pages, id, &Page::id);
    const bool nav = page != std::end(pages);
    const bool segment = isChoiceButton(id);
    const bool choiceHeading = !choices(id).empty();
    const bool active = nav && id == page_;
    const auto now = Clock::now();
    const float selectedAmount = segment ? (animate_ ? choiceMotion_[id - firstChoiceButton].value(now) :
        choiceMotion_[id - firstChoiceButton].to) : static_cast<float>(active);
    const bool featureRow = id >= IDC_FEATURE_MEDIA && id <= IDC_FEATURE_SYSTEM;
    const bool compactButton = isTestButton(id) || id == IDC_FEATURE_BACK ||
        id == IDC_FEATURE_UP || id == IDC_FEATURE_DOWN;
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool hot = !disabled && hovered(item.hwndItem);
    const auto base = nav ? sidebar : background;
    if (!canvas_.begin(item.hDC, item.rcItem, base)) return;
    const float phase = nav ? 1.0f : std::lerp(0.72f, 1.0f, pageOpacity());
    if (!choiceHeading && (active || !nav || hot || (item.itemState & (ODS_SELECTED | ODS_FOCUS)))) {
        canvas_.rounded(rect(area), static_cast<float>(px(6)),
                        segment && !hot && !(item.itemState & ODS_SELECTED)
                            ? blend(surface, selected, selectedAmount)
                            : blend(base, active || hot || (item.itemState & ODS_SELECTED) ? selected : surface, phase));
    }
    if (item.itemState & ODS_FOCUS) {
        auto focus = area;
        InflateRect(&focus, -px(2), -px(2));
        canvas_.focus(focus, static_cast<float>(px(5)), accent);
    }
    wchar_t text[96]{};
    GetWindowTextW(item.hwndItem, text, ARRAYSIZE(text));
    const bool appearanceTest = isTestButton(id) && testButtons[id - firstTestButton].test == SettingsTest::appearance;
    const auto* label = segment ? choices(choiceRowId(id))[choiceIndex(id)] :
        appearanceTest ? L"Test appearance" : isTestButton(id) ? L"Test" : nav ? (id == IDC_SETTINGS_PRIVACY ? L"Privacy" : page->title) : text;
    area.left += px(choiceHeading ? 1 : compactButton || segment ? 8 : 12);
    const auto ink = active ? accent : (disabled && !choiceHeading) || nav ? muted : foreground;
    canvas_.text(area, label, compactButton || segment ? canvas_.body() : canvas_.strong(),
                 segment ? blend(ink, accent, selectedAmount) : blend(base, ink, phase));
    if (featureRow) {
        area.left = area.right - px(21);
        canvas_.text(area, L"›", canvas_.body(), blend(base, muted, phase));
    } else if (id == IDC_CUSTOM_ACCENT) {
        wchar_t hex[16]{};
        swprintf_s(hex, L"#%06X", settings_.customAccent);
        area.left = area.right - px(89);
        const auto middle = (area.top + area.bottom) / 2;
        const RECT swatch{area.left, middle - px(7), area.left + px(14), middle + px(7)};
        const auto value = settings_.customAccent;
        canvas_.rounded(rect(swatch), static_cast<float>(px(4)), color(RGB(value >> 16, value >> 8, value)));
        area.left += px(21);
        canvas_.text(area, hex, canvas_.body(), blend(base, muted, phase));
    }
    canvas_.end();
}

LRESULT CALLBACK SettingsWindow::optionProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                                            UINT_PTR id, DWORD_PTR data) {
    if (message == WM_SETCURSOR && LOWORD(lparam) == HTCLIENT && IsWindowEnabled(window)) {
        SetCursor(LoadCursorW(nullptr, IDC_HAND));
        return TRUE;
    }
    if (message == WM_PAINT) {
        auto& self = *reinterpret_cast<SettingsWindow*>(data);
        for (const auto& option : options) {
            if (option.id != GetDlgCtrlID(window)) continue;
            PAINTSTRUCT ps{};
            self.drawOption(window, BeginPaint(window, &ps), option);
            EndPaint(window, &ps);
            return 0;
        }
    }
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_MOUSEMOVE) {
        TRACKMOUSEEVENT leave{sizeof(leave), TME_LEAVE, window, 0};
        TrackMouseEvent(&leave);
    }
    if (message == WM_NCDESTROY) RemoveWindowSubclass(window, optionProc, id);
    const auto result = DefSubclassProc(window, message, wparam, lparam);
    if (message == WM_SETFOCUS || message == WM_KILLFOCUS || message == BM_SETCHECK || message == WM_LBUTTONUP ||
        message == WM_KEYUP || message == BM_CLICK || message == WM_MOUSEMOVE || message == WM_MOUSELEAVE)
        InvalidateRect(window, nullptr, FALSE);
    return result;
}

INT_PTR CALLBACK SettingsWindow::dialog(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_INITDIALOG) {
        SetWindowLongPtrW(window, DWLP_USER, lparam);
        reinterpret_cast<SettingsWindow*>(lparam)->initialize(window);
        return TRUE;
    }
    auto* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(window, DWLP_USER));
    if (!self) return FALSE;
    switch (message) {
    case notificationAccessMessage:
        self->completeNotificationAccess(static_cast<NotificationAccess>(wparam));
        return TRUE;
    case WM_DESTROY:
        if (self->notificationRequest_) self->notificationRequest_.Cancel();
        break;
    case WM_CLOSE:
        EndDialog(window, 0);
        return TRUE;
    case WM_ACTIVATE:
        if (LOWORD(wparam) != WA_INACTIVE) self->sync();
        break;
    case WM_DPICHANGED:
        self->setDpi(HIWORD(wparam));
        InvalidateRect(window, nullptr, TRUE);
        break;
    case WM_SETTINGCHANGE: {
        BOOL motion = TRUE;
        SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &motion, 0);
        self->animate_ = motion != FALSE;
        self->redrawMotion();
        break;
    }
    case WM_TIMER:
        if (wparam == motionTimer) {
            self->redrawMotion();
            return TRUE;
        }
        break;
    case WM_ERASEBKGND:
        return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        self->paint(BeginPaint(window, &ps));
        EndPaint(window, &ps);
        return TRUE;
    }
    case WM_DRAWITEM:
        self->drawButton(*reinterpret_cast<DRAWITEMSTRUCT const*>(lparam));
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wparam) == IDCANCEL) {
            EndDialog(window, 0);
            return TRUE;
        }
        if (HIWORD(wparam) == BN_CLICKED) return self->command(LOWORD(wparam));
        break;
    }
    return FALSE;
}

}

int runSettingsWindow(HINSTANCE instance) {
    const auto mutex = CreateMutexW(nullptr, FALSE, L"Local\\ZenithDot.Settings");
    const auto error = GetLastError();
    if (!mutex) return 1;
    winrt::handle singleInstance{mutex};
    if (error == ERROR_ALREADY_EXISTS) {
        if (const auto existing = FindWindowW(L"#32770", title)) {
            if (IsIconic(existing)) ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        return 0;
    }
    SettingsWindow settings;
    const auto result = DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_SETTINGS), nullptr, SettingsWindow::dialog,
                                       reinterpret_cast<LPARAM>(&settings));
    if (result == -1) winrt::throw_last_error();
    return static_cast<int>(result);
}

}
