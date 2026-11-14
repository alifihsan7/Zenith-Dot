#pragma once

#include <windows.h>

#include <filesystem>
#include <string>

namespace zenith {

struct ScratchpadView {
    std::wstring text;
    bool saveFailed = false;
};

class Scratchpad {
public:
    static constexpr UINT dismissed = WM_APP + 9;
    static constexpr LPARAM explicitDismiss = 1;
    Scratchpad() = default;
    ~Scratchpad();
    Scratchpad(Scratchpad const&) = delete;
    Scratchpad& operator=(Scratchpad const&) = delete;

    void load();
    bool open(HWND owner, RECT bounds);
    void place(RECT bounds);
    void close(bool restoreFocus = true);
    bool scrollAt(POINT screen, int delta);
    // The editor follows the island's tinted surface; a change repaints it while open.
    void setBackground(COLORREF color);
    HWND window() const { return window_; }
    ScratchpadView const& view() const { return view_; }
    void clear();

private:
    static LRESULT CALLBACK dispatch(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK editInput(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
    void save();
    void write();

    ScratchpadView view_;
    std::filesystem::path file_;
    HWND owner_ = nullptr;
    HWND previous_ = nullptr;
    HWND window_ = nullptr;
    HWND edit_ = nullptr;
    HFONT font_ = nullptr;
    HBRUSH background_ = nullptr;
    COLORREF backgroundColor_ = RGB(23, 25, 28);
    bool dirty_ = false;
    int wheelRemainder_ = 0;
};

}
