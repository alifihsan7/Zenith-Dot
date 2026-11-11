#include "system.hpp"

#include <windows.h>
#include <ipifcons.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Networking.Connectivity.h>

namespace zenith {
namespace {

uint64_t ticks(FILETIME const& time) {
    return (static_cast<uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}

}

NetworkStatus readNetwork() {
    using namespace winrt::Windows::Networking::Connectivity;
    try {
        auto profile = NetworkInformation::GetInternetConnectionProfile();
        auto level = profile ? profile.GetNetworkConnectivityLevel() : NetworkConnectivityLevel::None;
        if (level != NetworkConnectivityLevel::InternetAccess) {
            for (const auto& candidate : NetworkInformation::GetConnectionProfiles()) {
                const auto candidateLevel = candidate.GetNetworkConnectivityLevel();
                if (candidateLevel <= level) continue;
                profile = candidate;
                level = candidateLevel;
                if (level == NetworkConnectivityLevel::InternetAccess) break;
            }
        }
        NetworkStatus status;
        switch (level) {
        case NetworkConnectivityLevel::None: return {NetworkAccess::disconnected};
        case NetworkConnectivityLevel::LocalAccess: status.access = NetworkAccess::local; break;
        case NetworkConnectivityLevel::ConstrainedInternetAccess: status.access = NetworkAccess::limited; break;
        case NetworkConnectivityLevel::InternetAccess: status.access = NetworkAccess::internet; break;
        }
        try {
            if (profile.IsWlanConnectionProfile()) {
                status.type = NetworkType::wifi;
                const auto wlan = profile.WlanConnectionProfileDetails();
                if (wlan) status.name = wlan.GetConnectedSsid();
            } else if (profile.IsWwanConnectionProfile()) {
                status.type = NetworkType::cellular;
            } else if (const auto adapter = profile.NetworkAdapter();
                       adapter && adapter.IanaInterfaceType() == IF_TYPE_ETHERNET_CSMACD) {
                status.type = NetworkType::ethernet;
            }
        } catch (winrt::hresult_error const&) {
            // Keep the connectivity status if Windows restricts access to adapter details.
        }
        return status;
    } catch (winrt::hresult_error const&) {
        return {};
    }
}

std::optional<bool> updateInternetStatus(std::optional<bool>& lastOnline, NetworkAccess access) {
    if (access == NetworkAccess::unknown) return std::nullopt;
    const bool online = access == NetworkAccess::internet;
    const auto previous = lastOnline;
    lastOnline = online;
    if (previous && *previous != online) return online;
    return std::nullopt;
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
