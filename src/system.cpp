#include "system.hpp"

#include <windows.h>

namespace zenith {
namespace {

uint64_t ticks(FILETIME const& time) {
    return (static_cast<uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}

}

SystemStats SystemMonitor::sample() {
    SystemStats stats;
    FILETIME idle{}, kernel{}, user{};
    if (GetSystemTimes(&idle, &kernel, &user)) {
        // Kernel time already includes idle time.
        const uint64_t idleNow = ticks(idle);
        const uint64_t totalNow = ticks(kernel) + ticks(user);
        const uint64_t busy = (totalNow - total_) - (idleNow - idle_);
        if (total_ != 0 && totalNow > total_) {
            stats.cpu = static_cast<unsigned>((busy * 100 + (totalNow - total_) / 2) / (totalNow - total_));
        }
        idle_ = idleNow;
        total_ = totalNow;
    }
    MEMORYSTATUSEX memory{sizeof(MEMORYSTATUSEX)};
    if (GlobalMemoryStatusEx(&memory)) stats.memory = memory.dwMemoryLoad;
    return stats;
}

}
