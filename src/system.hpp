#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace zenith {

enum class NetworkAccess { unknown, disconnected, local, limited, internet };
enum class NetworkType { other, wifi, ethernet, cellular };

struct NetworkStatus {
    NetworkAccess access = NetworkAccess::unknown;
    NetworkType type = NetworkType::other;
    std::wstring name;
    bool operator==(NetworkStatus const&) const = default;
};

NetworkStatus readNetwork();
// A missing result means the status was unknown, unchanged, or the first known sample.
std::optional<bool> updateInternetStatus(std::optional<bool>& lastOnline, NetworkAccess access);

struct SystemStats {
    unsigned cpu = 0;
    unsigned memory = 0;

    bool operator==(SystemStats const&) const = default;
};

// CPU use is measured between two calls, so the first sample after a pause reads since the last one.
class SystemMonitor {
public:
    SystemStats sample();

private:
    uint64_t idle_ = 0;
    uint64_t total_ = 0;
};

}
