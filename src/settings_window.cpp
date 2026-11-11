#include "settings.hpp"
#include "browser_tabs.hpp"
#include "resource.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <winrt/base.h>

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

void fill(HDC dc, RECT area, COLORREF color) {
    SetDCBrushColor(dc, color);
    FillRect(dc, &area, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}

void rounded(HDC dc, RECT area, int diameter, COLORREF color) {
    const auto brush = SelectObject(dc, GetStockObject(DC_BRUSH));
    const auto pen = SelectObject(dc, GetStockObject(NULL_PEN));
    SetDCBrushColor(dc, color);
    RoundRect(dc, area.left, area.top, area.right, area.bottom, diameter, diameter);
    SelectObject(dc, pen);
    SelectObject(dc, brush);
}

void text(HDC dc, RECT area, wchar_t const* value, HFONT font, COLORREF color,
          UINT format = DT_LEFT | DT_SINGLELINE | DT_VCENTER) {
    const auto oldFont = font ? SelectObject(dc, font) : nullptr;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, value, -1, &area, format);
    if (oldFont) SelectObject(dc, oldFont);
}

class SettingsWindow {
public:
    ~SettingsWindow() {
        for (const auto font : {body_, strong_, heading_}) if (font) DeleteObject(font);
    }
    static INT_PTR CALLBACK dialog(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

private:
    static LRESULT CALLBACK optionProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                                      UINT_PTR id, DWORD_PTR data);
    void initialize(HWND window);
    void createFonts(UINT dpi);
    void selectPage(int id);
    void sync();
    bool command(int id);
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
    HFONT body_{}, strong_{}, heading_{};
    UINT dpi_ = 96;
    int page_ = IDC_SETTINGS_GENERAL;
    std::wstring startup_;
};

void SettingsWindow::createFonts(UINT dpi) {
    for (const auto font : {body_, strong_, heading_}) if (font) DeleteObject(font);
    dpi_ = dpi;
    const auto font = [this](int points, int weight) {
        return CreateFontW(-MulDiv(points, dpi_, 72), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    };
    body_ = font(9, FW_NORMAL);
    strong_ = font(10, FW_SEMIBOLD);
    heading_ = font(22, FW_SEMIBOLD);
}

void SettingsWindow::initialize(HWND window) {
    window_ = window;
    createFonts(GetDpiForWindow(window));
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
        }
        return true;
    }
    return false;
}

void SettingsWindow::paint(HDC dc) {
    RECT client{};
    GetClientRect(window_, &client);
    fill(dc, client, background);
    const auto rail = units(0, 0, 122, 0);
    fill(dc, {0, 0, rail.right, client.bottom}, sidebar);
    fill(dc, {rail.right - 1, 0, rail.right, client.bottom}, border);
    const auto logo = units(13, 13, 13, 13);
    if (icon_) DrawIconEx(dc, logo.left, logo.top, icon_, px(30), px(30), 0, nullptr, DI_NORMAL);
    text(dc, units(38, 12, 115, 34), L"Zenith Dot", strong_, foreground);

    const auto& page = pages[page_ - IDC_SETTINGS_GENERAL];
    text(dc, units(144, 12, 418, 38), page.title, heading_, foreground);
    text(dc, units(145, 39, 418, 53), page.description, body_, muted);
    if (page.group) text(dc, units(145, 61, 418, 75), page.group, strong_, foreground);
    if (page_ == IDC_SETTINGS_FEATURES) {
        text(dc, units(145, 205, 418, 220), L"Timers", strong_, foreground);
    }
    if (page_ == IDC_SETTINGS_ABOUT) {
        text(dc, units(145, 70, 418, 92), L"Version " ZENITH_VERSION_TEXT, strong_, foreground);
    }
}

void SettingsWindow::drawOption(HWND control, HDC dc, Option const& option) {
    RECT row{};
    GetClientRect(control, &row);
    fill(dc, row, background);
    rounded(dc, row, px(12), surface);
    const bool enabled = IsWindowEnabled(control) != FALSE;
    const bool checked = SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED;
    const int right = row.right - px(15);
    const int middle = row.bottom / 2;
    const RECT track{right - px(34), middle - px(10), right, middle + px(10)};
    rounded(dc, track, px(20), checked && enabled ? accent : border);
    const int knob = checked ? track.right - px(16) : track.left + px(3);
    rounded(dc, {knob, middle - px(7), knob + px(14), middle + px(7)}, px(14),
            checked && enabled ? selected : muted);

    wchar_t label[96]{};
    GetWindowTextW(control, label, ARRAYSIZE(label));
    const int left = px(14);
    const int textRight = track.left - px(12);
    text(dc, {left, px(5), textRight, middle + px(1)}, label, strong_, enabled ? foreground : muted);
    text(dc, {left, middle, textRight, row.bottom - px(6)}, option.description, body_, muted,
         DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (GetFocus() == control) {
        InflateRect(&row, -px(2), -px(2));
        DrawFocusRect(dc, &row);
    }
}

void SettingsWindow::drawButton(DRAWITEMSTRUCT const& item) {
    auto rect = item.rcItem;
    const bool active = static_cast<int>(item.CtlID) == page_;
    const bool link = item.CtlID == IDC_SETTINGS_GITHUB;
    fill(item.hDC, rect, link ? background : sidebar);
    if (active || link || (item.itemState & (ODS_SELECTED | ODS_FOCUS))) {
        rounded(item.hDC, rect, px(10), active || link ? selected : surface);
    }
    if (item.itemState & ODS_FOCUS) {
        auto focus = rect;
        InflateRect(&focus, -px(2), -px(2));
        DrawFocusRect(item.hDC, &focus);
    }
    wchar_t label[64]{};
    GetWindowTextW(item.hwndItem, label, ARRAYSIZE(label));
    rect.left += px(12);
    text(item.hDC, rect, label, strong_, active || link ? accent : muted,
         DT_LEFT | DT_SINGLELINE | DT_VCENTER | ((item.itemState & ODS_NOACCEL) ? DT_HIDEPREFIX : 0));
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
        self->createFonts(HIWORD(wparam));
        InvalidateRect(window, nullptr, TRUE);
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
