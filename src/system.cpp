#include "system.hpp"

#include <windows.h>
#include <ipifcons.h>
#include <pdhmsg.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <string_view>
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
        case NetworkConnectivityLevel::None: return {NetworkAccess::disconnected, NetworkType::other, {}, {}};
        case NetworkConnectivityLevel::LocalAccess: status.access = NetworkAccess::local; break;
        case NetworkConnectivityLevel::ConstrainedInternetAccess: status.access = NetworkAccess::limited; break;
        case NetworkConnectivityLevel::InternetAccess: status.access = NetworkAccess::internet; break;
        }
        try {
            if (profile.IsWlanConnectionProfile()) {
                status.type = NetworkType::wifi;
                const auto wlan = profile.WlanConnectionProfileDetails();
                if (wlan) status.name = wlan.GetConnectedSsid();
                if (const auto bars = profile.GetSignalBars()) status.signalBars = std::min<unsigned>(bars.Value(), 5);
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

namespace {

bool validCounter(DWORD status) {
    return status == PDH_CSTATUS_VALID_DATA || status == PDH_CSTATUS_NEW_DATA;
}

unsigned percent(double value) {
    return static_cast<unsigned>(std::lround(std::clamp(value, 0.0, 100.0)));
}

std::optional<unsigned> gpuActivity(PDH_HCOUNTER counter) {
    if (!counter) return std::nullopt;
    DWORD bytes = 0, count = 0;
    constexpr DWORD format = PDH_FMT_DOUBLE | PDH_FMT_NOCAP100;
    if (PdhGetFormattedCounterArrayW(counter, format, &bytes, &count, nullptr) != PDH_MORE_DATA || !bytes)
        return std::nullopt;
    std::vector<std::byte> buffer(bytes);
    auto* entries = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
    if (PdhGetFormattedCounterArrayW(counter, format, &bytes, &count, entries) != ERROR_SUCCESS)
        return std::nullopt;

    // Windows reports one GPU engine instance per process. Group the same physical engine before taking the peak.
    std::map<std::wstring_view, double> engines;
    double busiest = 0;
    for (DWORD i = 0; i < count; ++i) {
        const auto& entry = entries[i];
        if (!validCounter(entry.FmtValue.CStatus) || !std::isfinite(entry.FmtValue.doubleValue)) continue;
        const std::wstring_view instance(entry.szName);
        const auto engine = instance.find(L"_luid_");
        const auto key = engine == std::wstring_view::npos ? instance : instance.substr(engine);
        busiest = std::max(busiest, engines[key] += entry.FmtValue.doubleValue);
    }
    return engines.empty() ? std::nullopt : std::optional{percent(busiest)};
}

}

SystemMonitor::~SystemMonitor() { stopExtended(); }

void SystemMonitor::stopExtended() {
    if (query_) PdhCloseQuery(query_);
    query_ = nullptr;
    gpu_ = nullptr;
    disks_.fill(nullptr);
    driveMask_ = 0;
}

SystemStats SystemMonitor::sample(bool extended) {
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
    if (!extended) return stats;
    if (!query_ && PdhOpenQueryW(nullptr, 0, &query_) == ERROR_SUCCESS)
        PdhAddEnglishCounterW(query_, LR"(\GPU Engine(*)\Utilization Percentage)", 0, &gpu_);

    const DWORD drives = GetLogicalDrives();
    DWORD fixedDrives = 0;
    for (size_t i = 0; i < disks_.size(); ++i) {
        const DWORD bit = 1u << i;
        const auto letter = static_cast<wchar_t>(L'A' + i);
        if (!(drives & bit)) {
            if (disks_[i]) PdhRemoveCounter(disks_[i]);
            disks_[i] = nullptr;
            continue;
        }
        const wchar_t root[]{letter, L':', L'\\', L'\0'};
        if (GetDriveTypeW(root) != DRIVE_FIXED) {
            if (disks_[i]) PdhRemoveCounter(disks_[i]);
            disks_[i] = nullptr;
            continue;
        }
        fixedDrives |= bit;
        if (query_ && !(driveMask_ & bit)) {
            const auto path = std::wstring(LR"(\LogicalDisk()") + letter + LR"(:)\% Idle Time)";
            PdhAddEnglishCounterW(query_, path.c_str(), 0, &disks_[i]);
        }
        stats.disks.push_back({letter, {}});
    }
    driveMask_ = query_ ? fixedDrives : 0;
    if (query_ && PdhCollectQueryData(query_) == ERROR_SUCCESS) {
        stats.gpu = gpuActivity(gpu_);
        for (auto& disk : stats.disks) {
            const auto counter = disks_[disk.letter - L'A'];
            PDH_FMT_COUNTERVALUE value{};
            if (counter && PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &value) == ERROR_SUCCESS &&
                validCounter(value.CStatus) && std::isfinite(value.doubleValue)) {
                disk.usage = percent(100.0 - value.doubleValue);
            }
        }
    }
    return stats;
}

}
