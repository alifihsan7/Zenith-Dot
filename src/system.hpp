#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <windows.h>
#include <pdh.h>

namespace zenith {

enum class NetworkAccess { unknown, disconnected, local, limited, internet };
enum class NetworkType { other, wifi, ethernet, cellular };

struct NetworkStatus {
    NetworkAccess access = NetworkAccess::unknown;
    NetworkType type = NetworkType::other;
    std::wstring name;
    std::optional<unsigned> signalBars;
    bool operator==(NetworkStatus const&) const = default;
};

NetworkStatus readNetwork();
// A missing result means the status was unknown, unchanged, or the first known sample.
std::optional<bool> updateInternetStatus(std::optional<bool>& lastOnline, NetworkAccess access);

struct DiskSpace {
    uint64_t totalBytes;
    uint64_t freeBytes;
    bool operator==(DiskSpace const&) const = default;
};

struct DiskCapacity {
    wchar_t letter;
    std::optional<DiskSpace> space;
    bool operator==(DiskCapacity const&) const = default;
};

struct SystemStats {
    unsigned cpu = 0;
    unsigned memory = 0;

    std::optional<unsigned> gpu;
    std::vector<DiskCapacity> disks;

    bool operator==(SystemStats const&) const = default;
};

// CPU use is measured between two calls, so the first sample after a pause reads since the last one.
class SystemMonitor {
public:
    SystemMonitor() = default;
    ~SystemMonitor();
    SystemMonitor(SystemMonitor const&) = delete;
    SystemMonitor& operator=(SystemMonitor const&) = delete;

    SystemStats sample(bool extended = true);
    void stopExtended();

private:
    PDH_HQUERY query_ = nullptr;
    PDH_HCOUNTER gpu_ = nullptr;
    uint64_t idle_ = 0;
    uint64_t total_ = 0;
};

}
