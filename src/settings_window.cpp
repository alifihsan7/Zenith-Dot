#include "settings.hpp"
#include "browser_tabs.hpp"
#include "resource.h"

#include <commctrl.h>
#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <winrt/base.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

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

struct Page {
    int id;
    wchar_t const* title;
    wchar_t const* description;
    wchar_t const* group;
};

constexpr Page pages[]{
    {IDC_SETTINGS_GENERAL, L"General", L"Choose how Zenith Dot starts on this PC.", L"Startup"},
    {IDC_SETTINGS_DISPLAY, L"Display", L"Control the island's motion and fullscreen behavior.", L"Island"},
    {IDC_SETTINGS_FEATURES, L"Features", L"Set what Drop Zone captures and how Focus runs.", L"Drop Zone"},
    {IDC_SETTINGS_ABOUT, L"About", L"Zenith Dot for Windows", nullptr},
};

struct Option {
    int id;
    int page;
    wchar_t const* description;
    bool Settings::*field;
    bool (Settings::*save)() const;
};

constexpr Option options[]{
    {IDC_START_WITH_WINDOWS, IDC_SETTINGS_GENERAL, L"Launch Zenith Dot when you sign in.", nullptr, nullptr},
    {IDC_ANIMATE_ISLAND, IDC_SETTINGS_DISPLAY, L"Animate pill and panel transitions when Windows allows motion.",
        &Settings::animateIsland, &Settings::saveDisplay},
    {IDC_HIDE_FULLSCREEN, IDC_SETTINGS_DISPLAY, L"Hide the island over fullscreen apps and games.",
        &Settings::hideInFullscreen, &Settings::saveDisplay},
    {IDC_ADD_COPIED_TEXT, IDC_SETTINGS_FEATURES, L"Keep copied text in Drop Zone for this session.",
        &Settings::autoAddText, &Settings::saveClipboard},
    {IDC_ADD_SCREENSHOTS, IDC_SETTINGS_FEATURES, L"Add screenshots and other clipboard images to Drop Zone.",
        &Settings::autoAddScreenshots, &Settings::saveClipboard},
    {IDC_CLIPBOARD_PREVIEWS, IDC_SETTINGS_FEATURES, L"Show a short preview in the Copied bubble.",
        &Settings::showClipboardPreviews, &Settings::saveClipboard},
    {IDC_AUTO_CYCLE, IDC_SETTINGS_FEATURES, L"Start the next Focus or Break phase automatically.",
        &Settings::autoCycle, &Settings::saveTimer},
};

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
    selectPage(page_);
    sync();
}

void SettingsWindow::selectPage(int id) {
    if (page_ != id && animate_) {
        pageStart_ = Clock::now();
        SetTimer(window_, motionTimer, 16, nullptr);
    }
    page_ = id;
    for (const auto& option : options) {
        ShowWindow(GetDlgItem(window_, option.id), option.page == id ? SW_SHOW : SW_HIDE);
    }
    ShowWindow(GetDlgItem(window_, IDC_SETTINGS_GITHUB), id == IDC_SETTINGS_ABOUT ? SW_SHOW : SW_HIDE);
    InvalidateRect(window_, nullptr, FALSE);
    for (const auto& page : pages) InvalidateRect(GetDlgItem(window_, page.id), nullptr, FALSE);
}

void SettingsWindow::sync() {
    const auto settings = Settings::load();
    for (const auto& option : options) {
        const bool checked = option.field ? settings.*option.field : startupEnabled(startup_);
        auto& motion = toggles_[&option - options];
        if (motion.to != static_cast<float>(checked)) motion = {static_cast<float>(checked), static_cast<float>(checked), {}};
        CheckDlgButton(window_, option.id, checked ? BST_CHECKED : BST_UNCHECKED);
        InvalidateRect(GetDlgItem(window_, option.id), nullptr, FALSE);
    }
}

bool SettingsWindow::command(int id) {
    for (const auto& page : pages) {
        if (id == page.id) {
            selectPage(id);
            sync();
            return true;
        }
    }
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
            if (saved) {
                if (const auto overlay = FindWindowW(overlayWindowClass, nullptr)) {
                    PostMessageW(overlay, settingsChangedMessage, 0, 0);
                }
            }
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
            InvalidateRect(GetDlgItem(window_, id), nullptr, FALSE);
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
        auto content = units(122, 0, 440, 292);
        InvalidateRect(window_, &content, FALSE);
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
    label(units(144, 12, 418, 38), page.title, canvas_.heading(), foreground);
    label(units(145, 39, 418, 53), page.description, canvas_.body(), muted);
    if (page.group) label(units(145, 61, 418, 75), page.group, canvas_.strong(), foreground);
    if (page_ == IDC_SETTINGS_FEATURES) {
        label(units(145, 205, 418, 220), L"Timers", canvas_.strong(), foreground);
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
    canvas_.rounded(rect(row), static_cast<float>(px(8)), blend(background, surface, phase));
    const bool enabled = IsWindowEnabled(control) != FALSE;
    const float checked = animate_ ? toggles_[&option - options].value(Clock::now()) : toggles_[&option - options].to;
    const int right = row.right - px(15);
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
    canvas_.text({left, px(5), textRight, middle + px(1)}, label, canvas_.strong(),
                 blend(surface, enabled ? foreground : muted, phase));
    canvas_.text({left, middle, textRight, row.bottom - px(6)}, option.description, canvas_.body(),
                 blend(surface, muted, phase));
    if (GetFocus() == control) {
        InflateRect(&row, -px(2), -px(2));
        canvas_.focus(row, static_cast<float>(px(7)), accent);
    }
    canvas_.end();
}

void SettingsWindow::drawButton(DRAWITEMSTRUCT const& item) {
    RECT area{0, 0, item.rcItem.right - item.rcItem.left, item.rcItem.bottom - item.rcItem.top};
    const bool active = static_cast<int>(item.CtlID) == page_;
    const bool link = item.CtlID == IDC_SETTINGS_GITHUB;
    if (!canvas_.begin(item.hDC, item.rcItem, link ? background : sidebar)) return;
    const float phase = link ? std::lerp(0.72f, 1.0f, pageOpacity()) : 1.0f;
    if (active || link || (item.itemState & (ODS_SELECTED | ODS_FOCUS))) {
        canvas_.rounded(rect(area), static_cast<float>(px(6)),
                        blend(link ? background : sidebar, active || link ? selected : surface, phase));
    }
    if (item.itemState & ODS_FOCUS) {
        auto focus = area;
        InflateRect(&focus, -px(2), -px(2));
        canvas_.focus(focus, static_cast<float>(px(5)), accent);
    }
    const auto* label = link ? L"View on GitHub" : pages[item.CtlID - IDC_SETTINGS_GENERAL].title;
    area.left += px(12);
    canvas_.text(area, label, canvas_.strong(), blend(link ? selected : sidebar, active || link ? accent : muted, phase));
    canvas_.end();
}

LRESULT CALLBACK SettingsWindow::optionProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                                            UINT_PTR id, DWORD_PTR data) {
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
    if (message == WM_NCDESTROY) RemoveWindowSubclass(window, optionProc, id);
    const auto result = DefSubclassProc(window, message, wparam, lparam);
    if (message == WM_SETFOCUS || message == WM_KILLFOCUS || message == BM_SETCHECK || message == WM_LBUTTONUP ||
        message == WM_KEYUP || message == BM_CLICK) InvalidateRect(window, nullptr, FALSE);
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
