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
    HWND window() const { return window_; }
    ScratchpadView const& view() const { return view_; }

private:
    static LRESULT CALLBACK dispatch(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK editInput(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
    void save();

    ScratchpadView view_;
    std::filesystem::path file_;
    HWND owner_ = nullptr;
    HWND previous_ = nullptr;
    HWND window_ = nullptr;
    HWND edit_ = nullptr;
    HFONT font_ = nullptr;
    HBRUSH background_ = nullptr;
    bool dirty_ = false;
};

}
