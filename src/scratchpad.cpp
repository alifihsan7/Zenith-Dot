#include "scratchpad.hpp"

#include <commctrl.h>
#include <shlobj.h>
#include <windowsx.h>
#include <winrt/base.h>

#include <algorithm>
#include <fstream>
#include <memory>

namespace zenith {
namespace {

constexpr wchar_t editorClass[] = L"ZenithDot.Scratchpad";
constexpr UINT_PTR saveTimer = 1;
constexpr UINT noteLimit = 32'768;
constexpr COLORREF editorBackground = RGB(22, 28, 36);

}

Scratchpad::~Scratchpad() { close(); }

void Scratchpad::load() {
    PWSTR folder = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &folder))) return;
    const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> release(folder, CoTaskMemFree);
    file_ = std::filesystem::path(folder) / L"Zenith Dot" / L"scratchpad.txt";
    std::ifstream input(file_, std::ios::binary | std::ios::ate);
    const auto size = input.tellg();
    if (size <= 0 || size > 4 * noteLimit) return;
    std::string bytes(static_cast<size_t>(size), '\0');
    if (!input.seekg(0).read(bytes.data(), size)) return;
    try {
        view_.text = std::wstring(winrt::to_hstring(bytes));
    } catch (winrt::hresult_error const&) {
        view_.saveFailed = true;
    }
}

bool Scratchpad::open(HWND owner, RECT bounds) {
    if (window_) return true;
    const auto instance = GetModuleHandleW(nullptr);
    WNDCLASSW type{};
    type.lpfnWndProc = dispatch;
    type.hInstance = instance;
    type.hCursor = LoadCursorW(nullptr, IDC_IBEAM);
    type.lpszClassName = editorClass;
    if (!RegisterClassW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    owner_ = owner;
    previous_ = GetForegroundWindow();
    const auto dpi = GetDpiForWindow(owner);
    font_ = CreateFontW(-MulDiv(14, dpi, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    background_ = CreateSolidBrush(editorBackground);
    window_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, editorClass, L"Scratchpad", WS_POPUP,
                              bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top,
                              owner, nullptr, instance, this);
    if (!window_) {
        close();
        return false;
    }

    edit_ = CreateWindowExW(0, L"EDIT", view_.text.c_str(),
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
        0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top, window_, nullptr, instance, nullptr);
    if (!edit_ || !SetWindowSubclass(edit_, editInput, 1, reinterpret_cast<DWORD_PTR>(this))) {
        close();
        return false;
    }
    SendMessageW(edit_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), FALSE);
    SendMessageW(edit_, EM_SETLIMITTEXT, noteLimit, 0);
    SendMessageW(edit_, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                 MAKELPARAM(MulDiv(7, dpi, 96), MulDiv(7, dpi, 96)));
    ShowWindow(window_, SW_SHOW);
    SetForegroundWindow(window_);
    SetFocus(edit_);
    POINT pointer{};
    RECT client{};
    int caret = GetWindowTextLengthW(edit_);
    if (GetCursorPos(&pointer) && ScreenToClient(edit_, &pointer) && GetClientRect(edit_, &client) &&
        PtInRect(&client, pointer)) {
        caret = LOWORD(SendMessageW(edit_, EM_CHARFROMPOS, 0, MAKELPARAM(pointer.x, pointer.y)));
    }
    SendMessageW(edit_, EM_SETSEL, caret, caret);
    SendMessageW(edit_, EM_SCROLLCARET, 0, 0);
    return true;
}

void Scratchpad::place(RECT bounds) {
    RECT current{};
    if (!window_ || !GetWindowRect(window_, &current) || EqualRect(&current, &bounds)) return;
    SetWindowPos(window_, nullptr, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

void Scratchpad::save() {
    if (!dirty_ || !edit_) return;
    const auto count = GetWindowTextLengthW(edit_);
    std::wstring text(static_cast<size_t>(count) + 1, L'\0');
    text.resize(GetWindowTextW(edit_, text.data(), count + 1));
    view_.text = std::move(text);
    write();
}

bool Scratchpad::toggleCheck(size_t offset) {
    auto& text = view_.text;
    if (window_ || offset + 3 >= text.size() || text[offset] != L'[' || text[offset + 2] != L']') return false;
    auto& mark = text[offset + 1];
    mark = mark == L' ' ? L'x' : L' ';
    write();
    return true;
}

bool Scratchpad::addItem() {
    auto& text = view_.text;
    if (window_ || text.size() + 6 > noteLimit) return false;
    if (!text.empty() && text.back() != L'\n') text += L"\r\n";
    text += L"[ ] ";
    write();
    return true;
}

bool Scratchpad::clearDone() {
    if (window_) return false;
    const auto& text = view_.text;
    std::wstring kept;
    kept.reserve(text.size());
    bool changed = false;
    for (size_t start = 0; start < text.size();) {
        const auto newline = text.find(L'\n', start);
        const size_t end = newline == std::wstring::npos ? text.size() : newline + 1;
        const bool done = end - start >= 4 && text[start] == L'[' && (text[start + 1] == L'x' || text[start + 1] == L'X') &&
                          text[start + 2] == L']' && text[start + 3] == L' ';
        if (done) changed = true;
        else kept.append(text, start, end - start);
        start = end;
    }
    if (!changed) return false;
    // A note that ended on a removed line should not keep a dangling line break.
    if (!text.ends_with(L'\n')) {
        while (!kept.empty() && (kept.back() == L'\n' || kept.back() == L'\r')) kept.pop_back();
    }
    view_.text = std::move(kept);
    write();
    return true;
}

void Scratchpad::write() {
    view_.saveFailed = true;
    if (file_.empty()) return;

    std::error_code error;
    std::filesystem::create_directories(file_.parent_path(), error);
    if (error) return;
    auto pending = file_;
    pending += L".tmp";
    try {
        const auto bytes = winrt::to_string(view_.text);
        std::ofstream output(pending, std::ios::binary | std::ios::trunc);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        output.close();
        if (output && MoveFileExW(pending.c_str(), file_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            dirty_ = false;
            view_.saveFailed = false;
        }
    } catch (winrt::hresult_error const&) {
        // Keep the draft in memory so a later edit or close can retry the save.
    }
}

void Scratchpad::close(bool restoreFocus) {
    if (window_) {
        save();
        const auto closing = window_;
        const bool restore = restoreFocus && GetForegroundWindow() == closing;
        window_ = nullptr;
        DestroyWindow(closing);
        edit_ = nullptr;
        if (restore && previous_ && previous_ != owner_ && IsWindow(previous_)) SetForegroundWindow(previous_);
    }
    if (font_) DeleteObject(font_);
    if (background_) DeleteObject(background_);
    font_ = nullptr;
    background_ = nullptr;
    wheelRemainder_ = 0;
}

bool Scratchpad::scrollAt(POINT screen, int delta) {
    RECT bounds{};
    if (!edit_ || !GetWindowRect(edit_, &bounds) || !PtInRect(&bounds, screen)) {
        wheelRemainder_ = 0;
        return false;
    }

    RECT format{};
    SendMessageW(edit_, EM_GETRECT, 0, reinterpret_cast<LPARAM>(&format));
    TEXTMETRICW metrics{};
    const auto dc = GetDC(edit_);
    if (dc) {
        const auto previous = font_ ? SelectObject(dc, font_) : nullptr;
        GetTextMetricsW(dc, &metrics);
        if (previous && previous != HGDI_ERROR) SelectObject(dc, previous);
        ReleaseDC(edit_, dc);
    }
    const auto lines = SendMessageW(edit_, EM_GETLINECOUNT, 0, 0);
    if (metrics.tmHeight <= 0 || lines <= std::max(1L, (format.bottom - format.top) / metrics.tmHeight)) {
        wheelRemainder_ = 0;
        return false;
    }

    if ((wheelRemainder_ > 0 && delta < 0) || (wheelRemainder_ < 0 && delta > 0)) wheelRemainder_ = 0;
    wheelRemainder_ += delta;
    const int steps = wheelRemainder_ / WHEEL_DELTA;
    wheelRemainder_ %= WHEEL_DELTA;
    if (steps) SendMessageW(edit_, EM_LINESCROLL, 0, -3 * steps);
    return true;
}

LRESULT CALLBACK Scratchpad::dispatch(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCCREATE) {
        const auto creation = reinterpret_cast<CREATESTRUCTW const*>(lparam);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(creation->lpCreateParams));
    }
    const auto self = reinterpret_cast<Scratchpad*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (!self) return DefWindowProcW(window, message, wparam, lparam);

    switch (message) {
    case WM_SIZE:
        if (self->edit_) MoveWindow(self->edit_, 0, 0, LOWORD(lparam), HIWORD(lparam), TRUE);
        return 0;
    case WM_CTLCOLOREDIT:
        SetTextColor(reinterpret_cast<HDC>(wparam), RGB(245, 246, 248));
        SetBkColor(reinterpret_cast<HDC>(wparam), editorBackground);
        return reinterpret_cast<LRESULT>(self->background_);
    case WM_ERASEBKGND: {
        RECT rect{};
        GetClientRect(window, &rect);
        FillRect(reinterpret_cast<HDC>(wparam), &rect, self->background_);
        return 1;
    }
    case WM_COMMAND:
        if (HIWORD(wparam) == EN_CHANGE && self->edit_) {
            self->dirty_ = true;
            SetTimer(window, saveTimer, 350, nullptr);
        }
        return 0;
    case WM_TIMER:
        KillTimer(window, saveTimer);
        self->save();
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wparam) == WA_INACTIVE && self->window_ == window) {
            PostMessageW(self->owner_, dismissed, reinterpret_cast<WPARAM>(window), 0);
        }
        break;
    case WM_CLOSE:
        PostMessageW(self->owner_, dismissed, reinterpret_cast<WPARAM>(window), explicitDismiss);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK Scratchpad::editInput(HWND edit, UINT message, WPARAM wparam, LPARAM lparam,
                                        UINT_PTR, DWORD_PTR context) {
    const auto self = reinterpret_cast<Scratchpad*>(context);
    if (message == WM_KEYDOWN && wparam == VK_ESCAPE) {
        PostMessageW(self->owner_, dismissed, reinterpret_cast<WPARAM>(self->window_), explicitDismiss);
        return 0;
    }
    if (message == WM_CHAR && wparam == 1) {
        SendMessageW(edit, EM_SETSEL, 0, -1);
        return 0;
    }
    if (message == WM_MOUSEWHEEL) {
        const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (!self->scrollAt(point, GET_WHEEL_DELTA_WPARAM(wparam))) {
            PostMessageW(self->owner_, WM_MOUSEWHEEL, wparam, lparam);
        }
        return 0;
    }
    return DefSubclassProc(edit, message, wparam, lparam);
}

}
