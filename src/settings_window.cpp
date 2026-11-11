#include "settings.hpp"
#include "browser_tabs.hpp"
#include "resource.h"

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
constexpr auto toggleDuration = 160ms;
constexpr UINT_PTR motionTimer = 1;
constexpr UINT_PTR previewTimer = 2;

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
    {IDC_ANIMATE_ISLAND, IDC_SETTINGS_DISPLAY, L"Animate pill and panel transitions when Windows allows motion.",
        &Settings::animateIsland, &Settings::saveDisplay},
    {IDC_HIDE_FULLSCREEN, IDC_SETTINGS_DISPLAY, L"Hide the island over fullscreen apps and games.",
        &Settings::hideInFullscreen, &Settings::saveDisplay},
    {IDC_ADD_COPIED_TEXT, IDC_SETTINGS_FEATURES, L"Keep copied text in Drop Zone for this session.",
        &Settings::autoAddText, &Settings::saveClipboard, IDC_FEATURE_DROP},
    {IDC_ADD_SCREENSHOTS, IDC_SETTINGS_FEATURES, L"Add screenshots and other clipboard images to Drop Zone.",
        &Settings::autoAddScreenshots, &Settings::saveClipboard, IDC_FEATURE_DROP},
    {IDC_CLIPBOARD_PREVIEWS, IDC_SETTINGS_FEATURES, L"Show a short preview in the Copied bubble.",
        &Settings::showClipboardPreviews, &Settings::saveClipboard, IDC_FEATURE_DROP},
    {IDC_AUTO_CYCLE, IDC_SETTINGS_FEATURES, L"Start the next Focus or Break phase automatically.",
        &Settings::autoCycle, &Settings::saveTimer, IDC_FEATURE_TIMERS},
    {IDC_FEATURE_ENABLED, IDC_SETTINGS_FEATURES, L"Show this feature in the feature bar.", nullptr, nullptr},
    {IDC_SHOW_WAVEFORM, IDC_SETTINGS_FEATURES, L"Show the animated bars during playback.",
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
constexpr int previewButtons[]{IDC_PREVIEW_DOT, IDC_PREVIEW_PILL, IDC_PREVIEW_PANEL, IDC_PREVIEW_PLAY,
    IDC_PREVIEW_COPY_TEXT, IDC_PREVIEW_COPY_IMAGE, IDC_PREVIEW_STATUS, IDC_PREVIEW_AUDIO};

constexpr wchar_t const* featureNames[]{L"Now Playing", L"Timers", L"Scratchpad", L"Drop Zone", L"System"};
constexpr wchar_t const* widthChoices[]{L"Fit to title", L"147 px minimum"};
constexpr wchar_t const* sizeChoices[]{L"Compact", L"Default", L"Large"};
constexpr wchar_t const* accentChoices[]{L"Album artwork", L"Zenith blue", L"Custom"};
constexpr wchar_t const* pauseChoices[]{L"Immediately", L"3 seconds", L"5 seconds", L"Keep the title"};
constexpr wchar_t const* focusChoices[]{L"15 minutes", L"25 minutes", L"45 minutes"};
constexpr wchar_t const* breakChoices[]{L"5 minutes", L"10 minutes", L"15 minutes"};
constexpr wchar_t const* appearanceSamples[]{L"Short title", L"Long title"};
constexpr wchar_t const* mediaSamples[]{L"Song title", L"Synced lyrics"};
constexpr wchar_t const* timerSamples[]{L"Focus", L"Break"};
constexpr int choiceControls[]{IDC_PILL_WIDTH, IDC_ISLAND_SIZE, IDC_ACCENT_MODE, IDC_CUSTOM_ACCENT,
    IDC_PAUSE_DELAY, IDC_FOCUS_DURATION, IDC_BREAK_DURATION};

std::span<wchar_t const* const> choices(int id) {
    switch (id) {
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
    void text(RECT area, wchar_t const* value, IDWriteTextFormat* font, D2D1_COLOR_F ink, bool wrap = false);
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

void Canvas::text(RECT area, wchar_t const* value, IDWriteTextFormat* font, D2D1_COLOR_F ink, bool wrap) {
    if (!font) return;
    brush_->SetColor(ink);
    if (wrap) font->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    target_->DrawText(value, static_cast<UINT32>(wcslen(value)), font, rect(area), brush_.get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    if (wrap) font->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
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
    void showControls();
    void openDetail(int id);
    void drawPreview();
    D2D1_SIZE_F previewSize(D2D1_SIZE_F target);
    std::span<wchar_t const* const> samples() const;
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
    unsigned sample_ = 0;
    int appearanceShape_ = IDC_PREVIEW_PILL;
    bool sampleTextAdded_ = false;
    bool sampleImageAdded_ = false;
    bool sampleCopyPreview_ = false;
    Settings settings_;
    Clock::time_point sampleStarted_{};
    Clock::time_point timerEnds_{};
    Clock::time_point previewResizeStart_{};
    D2D1_SIZE_F previewFrom_{};
    D2D1_SIZE_F previewTarget_{};
    bool animate_ = true;
    Clock::time_point pageStart_{};
    std::array<ToggleMotion, std::size(options)> toggles_{};
    std::wstring startup_;
};

void SettingsWindow::setDpi(UINT dpi) {
    dpi_ = dpi;
    canvas_.setDpi(dpi);
}

void SettingsWindow::initialize(HWND window) {
    window_ = window;
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
    for (const int id : previewButtons)
        SetWindowSubclass(GetDlgItem(window, id), optionProc, 0, reinterpret_cast<DWORD_PTR>(this));
    for (const int id : {IDC_SETTINGS_GITHUB, IDC_PREVIEW_ACTION, IDC_PREVIEW_TIMER, IDC_FEATURE_BACK, IDC_FEATURE_UP, IDC_FEATURE_DOWN,
                        IDC_FEATURE_MEDIA, IDC_FEATURE_TIMERS, IDC_FEATURE_SCRATCH, IDC_FEATURE_DROP, IDC_FEATURE_SYSTEM}) {
        SetWindowSubclass(GetDlgItem(window, id), optionProc, 0, reinterpret_cast<DWORD_PTR>(this));
    }
    SetWindowTextW(GetDlgItem(window, IDC_SAMPLE_NOTE), L"Review the first draft\r\nSend feedback by Friday");
    sync();
}

void SettingsWindow::selectPage(int id) {
    if (page_ == id && !detail_) return;
    page_ = id;
    openDetail(0);
}

void SettingsWindow::openDetail(int id) {
    detail_ = id;
    sample_ = 0;
    appearanceShape_ = IDC_PREVIEW_PILL;
    sampleTextAdded_ = sampleImageAdded_ = sampleCopyPreview_ = false;
    sampleStarted_ = {};
    timerEnds_ = {};
    previewResizeStart_ = {};
    previewTarget_ = {};
    SetWindowTextW(GetDlgItem(window_, IDC_PREVIEW_TIMER), L"Start");
    SetWindowTextW(GetDlgItem(window_, IDC_PREVIEW_PLAY), L"Pause");
    KillTimer(window_, previewTimer);
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
    for (const int id : choiceControls) {
        const bool visible = id == IDC_PAUSE_DELAY ? detail_ == IDC_FEATURE_MEDIA && enabled :
            id == IDC_FOCUS_DURATION || id == IDC_BREAK_DURATION ? detail_ == IDC_FEATURE_TIMERS && enabled :
            appearance && (id != IDC_CUSTOM_ACCENT || settings_.accentMode == 2);
        show(id, visible);
    }
    const int motionTop = settings_.accentMode == 2 ? 263 : 236;
    for (const auto id : {IDC_ANIMATE_ISLAND, IDC_HIDE_FULLSCREEN}) {
        const int y = motionTop + (id == IDC_HIDE_FULLSCREEN ? 32 : 0);
        const auto bounds = units(144, y, 456, y + 30);
        SetWindowPos(GetDlgItem(window_, id), nullptr, bounds.left, bounds.top, bounds.right - bounds.left,
                     bounds.bottom - bounds.top, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    for (size_t index = 0; index < settings_.features.size(); ++index) {
        const auto id = IDC_FEATURE_MEDIA + static_cast<int>(settings_.features[index]);
        const int y = 69 + 41 * static_cast<int>(index);
        const auto bounds = units(144, y, 413, y + 36);
        SetWindowPos(GetDlgItem(window_, id), nullptr, bounds.left, bounds.top, bounds.right - bounds.left,
                     bounds.bottom - bounds.top, SWP_NOZORDER | SWP_NOACTIVATE);
        const auto toggle = units(415, y, 456, y + 36);
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
    show(IDC_PREVIEW_ACTION, appearance || (enabled && !samples().empty()));
    for (const int id : {IDC_PREVIEW_DOT, IDC_PREVIEW_PILL, IDC_PREVIEW_PANEL}) show(id, appearance);
    show(IDC_PREVIEW_PLAY, enabled && detail_ == IDC_FEATURE_MEDIA);
    show(IDC_PREVIEW_COPY_TEXT, enabled && detail_ == IDC_FEATURE_DROP);
    show(IDC_PREVIEW_COPY_IMAGE, enabled && detail_ == IDC_FEATURE_DROP);
    show(IDC_PREVIEW_STATUS, enabled && detail_ == IDC_FEATURE_SYSTEM);
    show(IDC_PREVIEW_AUDIO, enabled && detail_ == IDC_FEATURE_SYSTEM);
    show(IDC_PREVIEW_TIMER, enabled && detail_ == IDC_FEATURE_TIMERS);
    show(IDC_SAMPLE_NOTE, enabled && detail_ == IDC_FEATURE_SCRATCH);
    show(IDC_SETTINGS_GITHUB, page_ == IDC_SETTINGS_ABOUT);
    RedrawWindow(window_, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void SettingsWindow::sync() {
    settings_ = Settings::load();
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

std::span<wchar_t const* const> SettingsWindow::samples() const {
    if (page_ == IDC_SETTINGS_DISPLAY) return appearanceSamples;
    switch (detail_) {
    case IDC_FEATURE_MEDIA: return mediaSamples;
    case IDC_FEATURE_TIMERS: return timerSamples;
    default: return {};
    }
}

unsigned SettingsWindow::currentChoice(int id) const {
    switch (id) {
    case IDC_PILL_WIDTH: return settings_.fixedPillMinimum;
    case IDC_ISLAND_SIZE: return settings_.islandSize;
    case IDC_ACCENT_MODE: return settings_.accentMode;
    case IDC_PAUSE_DELAY: return settings_.compactPause;
    case IDC_FOCUS_DURATION: return settings_.focusPreset;
    case IDC_BREAK_DURATION: return settings_.breakPreset;
    default: return sample_;
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
    const auto values = id == IDC_PREVIEW_ACTION ? samples() : choices(id);
    if (values.empty()) return false;
    const auto menu = CreatePopupMenu();
    if (!menu) return true;
    for (unsigned i = 0; i < values.size(); ++i) {
        AppendMenuW(menu, MF_STRING | (i == currentChoice(id) ? MF_CHECKED : 0), i + 1, values[i]);
    }
    RECT control{};
    GetWindowRect(GetDlgItem(window_, id), &control);
    const auto result = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTALIGN,
                                      control.right, control.bottom, 0, window_, nullptr);
    DestroyMenu(menu);
    if (!result) return true;
    const auto index = static_cast<unsigned>(result - 1);
    if (id == IDC_PREVIEW_ACTION) {
        sample_ = index;
        if (detail_ == IDC_FEATURE_TIMERS) {
            KillTimer(window_, previewTimer);
            timerEnds_ = {};
            SetWindowTextW(GetDlgItem(window_, IDC_PREVIEW_TIMER), L"Start");
        }
        RedrawWindow(window_, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
        return true;
    }
    auto updated = Settings::load();
    switch (id) {
    case IDC_PILL_WIDTH: updated.fixedPillMinimum = index != 0; break;
    case IDC_ISLAND_SIZE: updated.islandSize = index; break;
    case IDC_ACCENT_MODE: updated.accentMode = index; break;
    case IDC_PAUSE_DELAY: updated.compactPause = index; break;
    case IDC_FOCUS_DURATION: updated.focusPreset = index; break;
    case IDC_BREAK_DURATION: updated.breakPreset = index; break;
    }
    if (updated.saveIsland()) notifyOverlay();
    else MessageBoxW(window_, L"Couldn’t save this setting.", title, MB_OK | MB_ICONWARNING);
    sync();
    if (detail_ == IDC_FEATURE_TIMERS) {
        timerEnds_ = {};
        KillTimer(window_, previewTimer);
        SetWindowTextW(GetDlgItem(window_, IDC_PREVIEW_TIMER), L"Start");
    }
    if (id == IDC_PAUSE_DELAY && sampleStarted_ != Clock::time_point{}) {
        KillTimer(window_, previewTimer);
        const auto due = sampleStarted_ + std::chrono::seconds(settings_.pauseSeconds());
        if (settings_.compactPause != 3 && Clock::now() < due) {
            const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(due - Clock::now());
            SetTimer(window_, previewTimer, static_cast<UINT>(std::max(1ms, remaining).count()), nullptr);
        }
    }
    return true;
}

bool SettingsWindow::command(int id) {
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
    if (page_ == IDC_SETTINGS_DISPLAY && id >= IDC_PREVIEW_DOT && id <= IDC_PREVIEW_PANEL) {
        appearanceShape_ = id;
        RedrawWindow(window_, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
        return true;
    }
    if (id == IDC_PREVIEW_PLAY && detail_ == IDC_FEATURE_MEDIA) {
        KillTimer(window_, previewTimer);
        const bool pause = sampleStarted_ == Clock::time_point{};
        sampleStarted_ = pause ? Clock::now() : Clock::time_point{};
        SetWindowTextW(GetDlgItem(window_, id), pause ? L"Play" : L"Pause");
        if (pause && settings_.pauseSeconds() > 0) SetTimer(window_, previewTimer, settings_.pauseSeconds() * 1000, nullptr);
        InvalidateRect(window_, nullptr, FALSE);
        return true;
    }
    if (detail_ == IDC_FEATURE_DROP && (id == IDC_PREVIEW_COPY_TEXT || id == IDC_PREVIEW_COPY_IMAGE)) {
        sampleCopyPreview_ = id == IDC_PREVIEW_COPY_TEXT;
        sampleTextAdded_ |= sampleCopyPreview_ && settings_.autoAddText;
        sampleImageAdded_ |= !sampleCopyPreview_ && settings_.autoAddScreenshots;
        InvalidateRect(window_, nullptr, FALSE);
        return true;
    }
    if (detail_ == IDC_FEATURE_SYSTEM && (id == IDC_PREVIEW_STATUS || id == IDC_PREVIEW_AUDIO)) {
        sample_ = id == IDC_PREVIEW_AUDIO;
        RedrawWindow(window_, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
        return true;
    }
    if (id == IDC_PREVIEW_TIMER && detail_ == IDC_FEATURE_TIMERS) {
        const bool start = timerEnds_ == Clock::time_point{};
        timerEnds_ = start ? Clock::now() + std::chrono::minutes(sample_ == 0 ? settings_.focusMinutes() : settings_.breakMinutes())
                           : Clock::time_point{};
        SetWindowTextW(GetDlgItem(window_, id), start ? L"Reset" : L"Start");
        if (start) SetTimer(window_, previewTimer, 250, nullptr);
        else KillTimer(window_, previewTimer);
        InvalidateRect(window_, nullptr, FALSE);
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
    if (previewResizeStart_ != Clock::time_point{}) {
        auto preview = units(144, 62, 456, 147);
        InvalidateRect(window_, &preview, FALSE);
        if (animate_ && now - previewResizeStart_ < pageDuration) moving = true;
        else previewResizeStart_ = {};
    }
    if (pageStart_ != Clock::time_point{}) {
        auto content = units(122, 0, 478, 337);
        InvalidateRect(window_, &content, FALSE);
        for (const int id : choiceControls) InvalidateRect(GetDlgItem(window_, id), nullptr, FALSE);
        for (int id = IDC_FEATURE_MEDIA; id <= IDC_FEATURE_SYSTEM; ++id)
            InvalidateRect(GetDlgItem(window_, id), nullptr, FALSE);
        InvalidateRect(GetDlgItem(window_, IDC_PREVIEW_ACTION), nullptr, FALSE);
        if (page_ == IDC_SETTINGS_ABOUT) InvalidateRect(GetDlgItem(window_, IDC_SETTINGS_GITHUB), nullptr, FALSE);
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

D2D1_SIZE_F SettingsWindow::previewSize(D2D1_SIZE_F target) {
    const auto now = Clock::now();
    const auto shown = [&] {
        const float t = animate_ ? progress(previewResizeStart_, pageDuration, now) : 1;
        return D2D1::SizeF(std::lerp(previewFrom_.width, previewTarget_.width, t),
                           std::lerp(previewFrom_.height, previewTarget_.height, t));
    };
    if (target.width != previewTarget_.width || target.height != previewTarget_.height) {
        previewFrom_ = previewTarget_.width > 0 ? shown() : target;
        previewResizeStart_ = animate_ && previewTarget_.width > 0 ? now : Clock::time_point{};
        previewTarget_ = target;
        if (previewResizeStart_ != Clock::time_point{}) SetTimer(window_, motionTimer, 16, nullptr);
    }
    return shown();
}

void SettingsWindow::drawPreview() {
    const auto bounds = units(144, 62, 456, 147);
    canvas_.rounded(rect(bounds), static_cast<float>(px(12)), color(RGB(34, 46, 63)));
    const bool appearance = page_ == IDC_SETTINGS_DISPLAY;
    if (detail_ && !settings_.enabled(feature())) {
        canvas_.text(units(154, 92, 446, 112), L"Feature hidden from the island", canvas_.strong(), color(muted));
        return;
    }
    if (detail_ == IDC_FEATURE_SCRATCH) {
        canvas_.text(units(157, 72, 443, 90), L"Scratchpad", canvas_.strong(), color(foreground));
        return;
    }
    const auto rgb = settings_.customAccent;
    const auto tint = settings_.accentMode == 2 ? RGB(rgb >> 16, rgb >> 8, rgb) :
        settings_.accentMode == 1 ? accent : RGB(233, 170, 130);
    const bool longTitle = appearance && sample_ == 1;
    const auto* track = longTitle ? L"A Walk Through the City After Midnight (Riverside Live Session)" : L"5% TINT";
    if (detail_ == IDC_FEATURE_DROP || detail_ == IDC_FEATURE_SYSTEM || (appearance && appearanceShape_ == IDC_PREVIEW_PANEL)) {
        const int inset = appearance ? static_cast<int>((1.12f - settings_.islandScale()) * 140) : 0;
        const bool mediaPanel = appearance;
        const auto panel = units(155 + inset, mediaPanel ? 68 : 83, 445 - inset, 118);
        auto shell = rect(panel);
        if (mediaPanel) {
            const auto size = previewSize(D2D1::SizeF(shell.right - shell.left, shell.bottom - shell.top));
            const float center = (shell.left + shell.right) / 2;
            shell = D2D1::RectF(center - size.width / 2, shell.top, center + size.width / 2, shell.top + size.height);
        }
        canvas_.rounded(shell, std::min(static_cast<float>(px(10)), (shell.bottom - shell.top) / 2), color(RGB(8, 10, 14)));
        if (mediaPanel && previewResizeStart_ != Clock::time_point{} &&
            progress(previewResizeStart_, pageDuration, Clock::now()) < 0.92f) return;
        const auto first = units(163 + inset, 85, 436 - inset, 100);
        const auto second = units(163 + inset, 100, 436 - inset, 114);
        if (detail_ == IDC_FEATURE_DROP) {
            canvas_.text(first, L"Drop Zone  ·  Project brief.pdf", canvas_.strong(), color(foreground));
            std::wstring items;
            if (sampleImageAdded_) items = L"Screenshot.png";
            if (sampleTextAdded_) {
                if (!items.empty()) items += L"   ·   ";
                items += L"Review the first draft";
            }
            canvas_.text(second, items.empty() ? L"Drop files or text here" : items.c_str(), canvas_.body(), color(muted));
            if (sampleCopyPreview_ && settings_.showClipboardPreviews) {
                canvas_.text(units(163, 67, 436, 82), L"Copied  ·  Review the first draft", canvas_.body(), color(foreground));
            }
        } else if (detail_ == IDC_FEATURE_SYSTEM) {
            canvas_.text(first, sample_ == 1 ? L"Audio output" : L"System", canvas_.strong(), color(foreground));
            canvas_.text(second, sample_ == 1 ? L"Speakers  ·  Volume 40%" : L"CPU 12%   ·   RAM 7.2 GB   ·   Battery 76%",
                         canvas_.body(), color(muted));
        } else {
            const auto artwork = units(163 + inset, 73, 180 + inset, 90);
            canvas_.rounded(rect(artwork), static_cast<float>(px(5)), blend(surface, tint, 0.33f));
            canvas_.text(artwork, L" ♪", canvas_.body(), color(tint));
            canvas_.text(units(186 + inset, 69, 436 - inset, longTitle ? 89 : 85), track,
                         canvas_.strong(), color(foreground), longTitle);
            canvas_.text(units(186 + inset, longTitle ? 89 : 86, 436 - inset, 97),
                         longTitle ? L"Demo track   ·   0:58 / 2:16" : L"Travis Scott   ·   0:58 / 2:16", canvas_.body(), color(muted));
            canvas_.fill(units(163 + inset, 98, 436 - inset, 100), border);
            canvas_.fill(units(163 + inset, 98, 285, 100), tint);
            canvas_.text(units(272, 101, 287, 116), L"‹", canvas_.strong(), color(foreground));
            canvas_.rounded(rect(units(290, 101, 310, 116)), static_cast<float>(px(12)), color(tint));
            canvas_.text(units(296, 101, 309, 116), L"Ⅱ", canvas_.body(), color(background));
            canvas_.text(units(319, 101, 335, 116), L"›", canvas_.strong(), color(foreground));
        }
        return;
    }

    const bool timer = detail_ == IDC_FEATURE_TIMERS;
    const bool paused = detail_ == IDC_FEATURE_MEDIA && sampleStarted_ != Clock::time_point{};
    const bool compact = paused && settings_.compactPause != 3 &&
        Clock::now() - sampleStarted_ >= std::chrono::seconds(settings_.pauseSeconds());
    const bool lyrics = detail_ == IDC_FEATURE_MEDIA && sample_ == 1 && settings_.showPillLyrics && !paused;
    const bool dot = appearance && appearanceShape_ == IDC_PREVIEW_DOT;
    const int width = dot ? 13 : compact ? 50 : longTitle ? 330 : lyrics ? 318 :
        timer ? 132 : settings_.fixedPillMinimum ? 147 : 114;
    const int height = dot ? 13 : compact ? 28 : longTitle ? 48 : 34;
    const float scale = settings_.islandScale();
    const auto now = Clock::now();
    const auto [span, tall] = previewSize(D2D1::SizeF(px(width) * scale, px(height) * scale));
    const float x = (bounds.left + bounds.right - span) / 2;
    const float y = appearance ? static_cast<float>(units(0, 68, 0, 68).top) : bounds.top + px(36) - tall / 2;
    const auto pill = D2D1::RectF(x, y, x + span, y + tall);
    canvas_.rounded(pill, tall / 2, color(RGB(8, 10, 14)));
    if (dot) return;
    const auto value = [&](int left, int right) {
        return RECT{static_cast<LONG>(x) + px(left), static_cast<LONG>(y),
                    static_cast<LONG>(x + span) - px(right), static_cast<LONG>(y + tall)};
    };
    std::wstring timerLabel;
    if (timer) {
        const auto seconds = timerEnds_ == Clock::time_point{} ?
            60 * (sample_ == 0 ? settings_.focusMinutes() : settings_.breakMinutes()) :
            std::max(0ll, static_cast<long long>(std::chrono::ceil<std::chrono::seconds>(timerEnds_ - now).count()));
        timerLabel = sample_ == 0 ? L"Focus   " : L"Break   ";
        timerLabel += std::to_wstring(seconds / 60) + (seconds % 60 < 10 ? L":0" : L":") + std::to_wstring(seconds % 60);
    }
    const auto* label = timer ? timerLabel.c_str() : lyrics ? L"City lights are fading, but I’m still wide awake" : track;
    if (!lyrics && !timer) {
        const float art = static_cast<float>(px(16));
        canvas_.rounded(D2D1::RectF(x + px(7), y + (tall - art) / 2, x + px(7) + art, y + (tall + art) / 2),
                        static_cast<float>(px(4)), blend(surface, tint, 0.33f));
        canvas_.text(value(9, 18), L"♪", canvas_.body(), color(tint));
    }
    if (!compact) canvas_.text(value(lyrics || timer ? 12 : 29, 24), label, canvas_.body(), color(foreground), longTitle);
    if (!lyrics && !timer && (paused || settings_.showWaveform)) {
        const int heights[]{6, 12, 9, 5};
        for (int i = 0; i < (paused ? 2 : 4); ++i) {
            const float barX = x + span - px((paused ? 16 : 19) - 3 * i);
            const float barHeight = static_cast<float>(px(paused ? 9 : heights[i]));
            canvas_.rounded(D2D1::RectF(barX, y + (tall - barHeight) / 2, barX + px(2), y + (tall + barHeight) / 2),
                            1, color(tint));
        }
    }
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
    const auto& page = pages[page_ - IDC_SETTINGS_GENERAL];
    if (detail_) {
        label(units(144, 31, 456, 57), featureNames[static_cast<unsigned>(feature())], canvas_.heading(), foreground);
        const auto position = std::ranges::find(settings_.features, feature()) - settings_.features.begin();
        const auto order = L"Feature bar position  " + std::to_wstring(position + 1) + L" of " +
            std::to_wstring(settings_.features.size());
        label(units(145, 296, 323, 319), order.c_str(), canvas_.body(), muted);
    } else {
        label(units(144, 12, 456, 38), page.title, canvas_.heading(), foreground);
        label(units(145, 39, 456, 53), page.description, canvas_.body(), muted);
        if (page.group) label(units(145, 61, 456, 75), page.group, canvas_.strong(), foreground);
    }
    if (detail_ || page_ == IDC_SETTINGS_DISPLAY) drawPreview();
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
        canvas_.text({left, middle, textRight, row.bottom - px(6)}, option.description, canvas_.body(),
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
    const bool nav = id >= IDC_SETTINGS_GENERAL && id <= IDC_SETTINGS_ABOUT;
    const bool previewAction = std::ranges::find(previewButtons, id) != std::end(previewButtons);
    const bool active = (nav && id == page_) ||
        (page_ == IDC_SETTINGS_DISPLAY && id == appearanceShape_) ||
        (detail_ == IDC_FEATURE_SYSTEM && id == (sample_ == 0 ? IDC_PREVIEW_STATUS : IDC_PREVIEW_AUDIO));
    const bool featureRow = id >= IDC_FEATURE_MEDIA && id <= IDC_FEATURE_SYSTEM;
    const bool compactButton = previewAction || id == IDC_PREVIEW_ACTION || id == IDC_PREVIEW_TIMER || id == IDC_FEATURE_BACK ||
        id == IDC_FEATURE_UP || id == IDC_FEATURE_DOWN;
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool hot = !disabled && hovered(item.hwndItem);
    const auto base = nav ? sidebar : previewAction || id == IDC_PREVIEW_ACTION || id == IDC_PREVIEW_TIMER ? RGB(34, 46, 63) : background;
    if (!canvas_.begin(item.hDC, item.rcItem, base)) return;
    const float phase = nav ? 1.0f : std::lerp(0.72f, 1.0f, pageOpacity());
    if (active || !nav || hot || (item.itemState & (ODS_SELECTED | ODS_FOCUS))) {
        canvas_.rounded(rect(area), static_cast<float>(px(6)),
                        blend(base, active || hot || (item.itemState & ODS_SELECTED) ? selected : surface, phase));
    }
    if (item.itemState & ODS_FOCUS) {
        auto focus = area;
        InflateRect(&focus, -px(2), -px(2));
        canvas_.focus(focus, static_cast<float>(px(5)), accent);
    }
    wchar_t text[96]{};
    GetWindowTextW(item.hwndItem, text, ARRAYSIZE(text));
    const auto values = id == IDC_PREVIEW_ACTION ? samples() : choices(id);
    const auto* label = nav ? pages[id - IDC_SETTINGS_GENERAL].title :
        id == IDC_PREVIEW_ACTION && !values.empty() ? values[sample_] : text;
    area.left += px(compactButton ? 8 : 12);
    const auto ink = active ? accent : disabled || nav ? muted : foreground;
    canvas_.text(area, label, compactButton ? canvas_.body() : canvas_.strong(), blend(base, ink, phase));
    if (featureRow || id == IDC_CUSTOM_ACCENT || (!values.empty() && id != IDC_PREVIEW_ACTION)) {
        wchar_t hex[16]{};
        const wchar_t* value = nullptr;
        if (id == IDC_CUSTOM_ACCENT) {
            swprintf_s(hex, L"#%06X", settings_.customAccent);
            value = hex;
        } else if (featureRow) {
            value = L"›";
        } else value = values[currentChoice(id)];
        area.left = area.right - px(featureRow ? 21 : 126);
        area.right -= px(8);
        if (!featureRow) {
            auto picker = area;
            picker.top += px(5);
            picker.bottom -= px(5);
            canvas_.rounded(rect(picker), static_cast<float>(px(5)), color(hot ? border : background));
            area.left += px(8);
            auto arrow = area;
            arrow.left = arrow.right - px(13);
            canvas_.text(arrow, L"⌄", canvas_.body(), color(muted));
            area.right -= px(14);
        }
        canvas_.text(area, value, canvas_.body(), blend(base, muted, phase));
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
        if (wparam == previewTimer) {
            if (self->timerEnds_ == Clock::time_point{} || Clock::now() >= self->timerEnds_) KillTimer(window, previewTimer);
            InvalidateRect(window, nullptr, FALSE);
            return TRUE;
        }
        break;
    case WM_CTLCOLOREDIT: {
        const auto dc = reinterpret_cast<HDC>(wparam);
        SetTextColor(dc, foreground);
        SetBkColor(dc, surface);
        SetDCBrushColor(dc, surface);
        return reinterpret_cast<INT_PTR>(GetStockObject(DC_BRUSH));
    }
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
