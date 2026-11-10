#pragma once

#include "media.hpp"

#include <map>
#include <optional>

namespace zenith {

inline constexpr wchar_t overlayWindowClass[] = L"ZenithDot.Overlay";
bool browserHostRequested();
int runBrowserHost(HINSTANCE instance);

struct BrowserDownload {
    std::wstring filename;
    unsigned count = 0;
    int progress = -1;
    bool paused = false;
    std::optional<unsigned> completedId;
};

class BrowserTabs {
public:
    void setWindow(HWND window) { window_ = window; }
    bool receive(HWND sender, COPYDATASTRUCT const& message);
    bool prune();
    bool select(std::wstring const& id);
    MediaState combine(MediaState const& native) const;
    void control(std::wstring const& source, MediaCommand command) const;
    void seek(std::wstring const& source, double seconds) const;
    void activate(std::wstring const& source) const;
    BrowserDownload const* download() const;
    void showDownload() const;

private:
    struct Tab {
        HWND host;
        unsigned number;
        MediaState media;
    };

    Tab const* find(std::wstring const& id) const;
    std::map<HWND, BrowserDownload>::const_iterator downloadSource() const;
    void send(std::wstring const& source, std::wstring_view action, double seconds) const;

    std::vector<Tab> tabs_;
    std::map<HWND, BrowserDownload> downloads_;
    std::wstring selected_;
    HWND window_ = nullptr;
};

}
