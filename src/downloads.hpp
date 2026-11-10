#pragma once

#include <windows.h>

#include <array>
#include <cstddef>
#include <set>
#include <string>
#include <string_view>

namespace zenith {

class DownloadMonitor {
public:
    static constexpr UINT changed = WM_APP + 12;

    DownloadMonitor() = default;
    ~DownloadMonitor();
    DownloadMonitor(DownloadMonitor const&) = delete;
    DownloadMonitor& operator=(DownloadMonitor const&) = delete;

    void start(HWND window, std::wstring folder = {});
    void stop();
    bool readChanges();
    size_t pendingCount() const { return pending_.size(); }
    std::wstring_view pendingName() const;
    size_t completedCount() const { return pending_.empty() ? completedCount_ : 0; }
    void clearCompleted();
    void revealCompleted() const;

private:
    bool arm();
    bool apply(DWORD action, std::wstring_view name);

    std::wstring folder_;
    std::set<std::wstring, std::less<>> pending_;
    std::wstring renamedFrom_;
    std::wstring completedName_;
    size_t completedCount_ = 0;
    HANDLE directory_ = INVALID_HANDLE_VALUE;
    PTP_WAIT wait_ = nullptr;
    OVERLAPPED read_{};
    alignas(DWORD) std::array<std::byte, 32 * 1024> changes_{};
};

}
