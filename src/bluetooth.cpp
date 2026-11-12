#include "bluetooth.hpp"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <chrono>
#include <algorithm>
#include <map>
#include <mutex>
#include <string_view>
#include <utility>

namespace zenith {
namespace {

using namespace std::chrono_literals;
using winrt::Windows::Devices::Enumeration::DeviceInformation;
using winrt::Windows::Devices::Enumeration::DeviceInformationKind;
using winrt::Windows::Devices::Enumeration::DeviceInformationUpdate;
using winrt::Windows::Devices::Enumeration::DeviceWatcher;
using winrt::Windows::Devices::Enumeration::DeviceWatcherStatus;
using Properties = winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, winrt::Windows::Foundation::IInspectable>;

constexpr wchar_t connectedKey[] = L"System.Devices.Aep.IsConnected";
constexpr wchar_t containerKey[] = L"System.Devices.Aep.ContainerId";
constexpr wchar_t categoryKey[] = L"System.Devices.Aep.Category";
constexpr wchar_t majorClassKey[] = L"System.Devices.Aep.Bluetooth.Cod.Major";
// DEVPKEY_Bluetooth_Battery: published on the device nodes of hands-free and LE battery profiles.
constexpr wchar_t batteryKey[] = L"{104EA319-6EE2-4701-BD47-8DDBF425BBE5} 2";
// Hands-free profiles report battery a moment after the link comes up.
constexpr auto batteryDelay = 1500ms;

winrt::Windows::Foundation::IInspectable lookup(Properties const& properties, wchar_t const* key) {
    if (!properties || !properties.HasKey(key)) return nullptr;
    return properties.Lookup(key);
}

std::optional<bool> readBool(Properties const& properties, wchar_t const* key) {
    if (const auto value = lookup(properties, key).try_as<winrt::Windows::Foundation::IReference<bool>>()) {
        return value.Value();
    }
    return std::nullopt;
}

std::optional<unsigned> readNumber(winrt::Windows::Foundation::IInspectable const& value) {
    using winrt::Windows::Foundation::IReference;
    if (const auto byte = value.try_as<IReference<uint8_t>>()) return byte.Value();
    if (const auto word = value.try_as<IReference<uint16_t>>()) return word.Value();
    if (const auto number = value.try_as<IReference<uint32_t>>()) return number.Value();
    return std::nullopt;
}

std::wstring readContainer(Properties const& properties) {
    if (const auto value = lookup(properties, containerKey).try_as<winrt::Windows::Foundation::IReference<winrt::guid>>()) {
        return std::wstring(winrt::to_hstring(value.Value()));
    }
    return {};
}

bool isAudio(Properties const& properties) {
    bool categorized = false;
    if (const auto value = lookup(properties, categoryKey)
            .try_as<winrt::Windows::Foundation::IReferenceArray<winrt::hstring>>()) {
        for (const auto& category : value.Value()) {
            const std::wstring_view name(category);
            if (name.empty() || name == L"Unknown") continue;
            categorized = true;
            for (const auto word : {L"Headset", L"Headphone", L"Audio", L"Speaker"}) {
                if (name.find(word) != std::wstring_view::npos) return true;
            }
        }
    }
    // Class of Device major 4 is Audio/Video; trust it only when Windows gives no category.
    return !categorized && readNumber(lookup(properties, majorClassKey)) == 4u;
}

}

struct BluetoothMonitor::State {
    struct Endpoint {
        std::wstring container;
        std::wstring name;
        bool connected = false;
        bool audio = false;
    };

    std::mutex lock;
    HWND window = nullptr;
    bool stopped = false;
    int enumerating = 0;
    std::map<std::wstring, Endpoint> endpoints;          // by endpoint id
    std::map<std::wstring, bool> containers;             // last reported state per physical device
    struct BatterySample {
        std::chrono::steady_clock::time_point requested;
        std::optional<unsigned> value;
        bool pending = false;
    };
    std::map<std::wstring, BatterySample> batteries;
    std::vector<BluetoothEvent> events;
    std::vector<DeviceWatcher> watchers;
};

namespace {

winrt::Windows::Foundation::IAsyncOperation<int> readBattery(std::wstring container) {
    try {
        const auto query = L"System.Devices.ContainerId:=\"" + container + L"\"";
        const auto nodes = co_await DeviceInformation::FindAllAsync(
            query, std::vector<winrt::hstring>{batteryKey}, DeviceInformationKind::Device);
        for (const auto& node : nodes) {
            if (const auto level = readNumber(lookup(node.Properties(), batteryKey)); level && *level <= 100)
                co_return static_cast<int>(*level);
        }
    } catch (winrt::hresult_error const&) {
    }
    co_return -1;
}

winrt::fire_and_forget refreshBattery(std::weak_ptr<BluetoothMonitor::State> weak, std::wstring container,
                                   std::chrono::steady_clock::time_point requested) {
    const auto level = co_await readBattery(container);
    if (const auto state = weak.lock()) {
        std::lock_guard guard(state->lock);
        const auto sample = state->batteries.find(container);
        if (state->stopped || sample == state->batteries.end() ||
            sample->second.requested != requested) co_return;
        sample->second.pending = false;
        if (level >= 0) sample->second.value = static_cast<unsigned>(level);
    }
}

winrt::fire_and_forget announce(std::weak_ptr<BluetoothMonitor::State> weak, std::wstring container,
                                BluetoothEvent event) {
    try {
        if (event.connected) {
            co_await winrt::resume_after(batteryDelay);
            const auto level = co_await readBattery(container);
            if (level >= 0) event.battery = static_cast<unsigned>(level);
        }
    } catch (winrt::hresult_error const&) {
        // Announce without a battery level.
    }
    const auto state = weak.lock();
    if (!state) co_return;
    std::lock_guard guard(state->lock);
    // A device that dropped again during the battery delay has a newer event queued instead.
    const auto current = state->containers.find(container);
    if (state->stopped || current == state->containers.end() || current->second != event.connected) co_return;
    if (event.battery) {
        auto& battery = state->batteries[container];
        if (!battery.pending) battery.requested = std::chrono::steady_clock::now();
        battery.value = event.battery;
    }
    state->events.push_back(std::move(event));
    PostMessageW(state->window, BluetoothMonitor::changed, 0, 0);
}

// Records an endpoint change and returns the physical device's new state when it should be announced.
std::optional<std::pair<std::wstring, BluetoothEvent>> record(BluetoothMonitor::State& state, std::wstring const& id,
                                                              Properties const& properties,
                                                              DeviceInformation const* added, bool removed) {
    std::lock_guard guard(state.lock);
    if (state.stopped) return std::nullopt;
    if (removed && !state.endpoints.contains(id)) return std::nullopt;
    auto& endpoint = state.endpoints[id];
    if (added) {
        endpoint.name = std::wstring(added->Name());
        endpoint.container = readContainer(properties);
        endpoint.audio = isAudio(properties);
    }
    if (const auto connected = readBool(properties, connectedKey)) endpoint.connected = *connected;
    const auto container = endpoint.container.empty() ? id : endpoint.container;
    auto name = endpoint.name;
    if (removed) state.endpoints.erase(id);

    // One physical device can expose a classic and an LE endpoint; it counts as connected if either is.
    bool connected = false, audio = false, any = false;
    for (const auto& [key, other] : state.endpoints) {
        if ((other.container.empty() ? key : other.container) != container) continue;
        any = true;
        connected = connected || other.connected;
        audio = audio || other.audio;
        if (name.empty()) name = other.name;
    }
    if (!connected) state.batteries.erase(container);
    const auto known = state.containers.find(container);
    if (!any) {
        if (known != state.containers.end()) state.containers.erase(known);
        return std::nullopt;
    }
    if (known == state.containers.end()) {
        state.containers.emplace(container, connected);
        // Devices found by the first enumeration were connected before Zenith started.
        if (state.enumerating > 0 || !connected) return std::nullopt;
    } else {
        if (known->second == connected) return std::nullopt;
        known->second = connected;
    }
    if (name.empty()) name = L"Bluetooth device";
    return std::pair{container, BluetoothEvent{std::move(name), connected, audio, std::nullopt}};
}

void update(std::shared_ptr<BluetoothMonitor::State> const& state, std::wstring const& id, Properties const& properties,
            DeviceInformation const* added, bool removed) {
    // announce takes the lock itself, so it runs after record has released it.
    if (auto change = record(*state, id, properties, added, removed)) {
        announce(state, std::move(change->first), std::move(change->second));
    }
}

}

BluetoothMonitor::BluetoothMonitor() : state_(std::make_shared<State>()) {}

BluetoothMonitor::~BluetoothMonitor() { stop(); }

void BluetoothMonitor::start(HWND window) {
    using winrt::Windows::Devices::Bluetooth::BluetoothDevice;
    using winrt::Windows::Devices::Bluetooth::BluetoothLEDevice;
    state_->window = window;
    std::weak_ptr<State> weak = state_;
    try {
        const winrt::hstring selectors[]{BluetoothDevice::GetDeviceSelectorFromPairingState(true),
                                         BluetoothLEDevice::GetDeviceSelectorFromPairingState(true)};
        for (const auto& selector : selectors) {
            auto watcher = DeviceInformation::CreateWatcher(selector,
                std::vector<winrt::hstring>{connectedKey, containerKey, categoryKey, majorClassKey},
                DeviceInformationKind::AssociationEndpoint);
            watcher.Added([weak](DeviceWatcher const&, DeviceInformation const& info) {
                if (const auto state = weak.lock()) update(state, std::wstring(info.Id()), info.Properties(), &info, false);
            });
            watcher.Updated([weak](DeviceWatcher const&, DeviceInformationUpdate const& change) {
                if (const auto state = weak.lock()) update(state, std::wstring(change.Id()), change.Properties(), nullptr, false);
            });
            watcher.Removed([weak](DeviceWatcher const&, DeviceInformationUpdate const& change) {
                if (const auto state = weak.lock()) update(state, std::wstring(change.Id()), nullptr, nullptr, true);
            });
            watcher.EnumerationCompleted([weak](DeviceWatcher const&, auto const&) {
                if (const auto state = weak.lock()) {
                    std::lock_guard guard(state->lock);
                    --state->enumerating;
                }
            });
            {
                std::lock_guard guard(state_->lock);
                ++state_->enumerating;
                state_->watchers.push_back(watcher);
            }
            watcher.Start();
        }
    } catch (winrt::hresult_error const& error) {
        // Without a Bluetooth stack there is nothing to announce.
        OutputDebugStringW(error.message().c_str());
    }
}

void BluetoothMonitor::stop() {
    std::vector<DeviceWatcher> watchers;
    {
        std::lock_guard guard(state_->lock);
        state_->stopped = true;
        watchers.swap(state_->watchers);
    }
    for (auto& watcher : watchers) {
        try {
            const auto status = watcher.Status();
            if (status == DeviceWatcherStatus::Started || status == DeviceWatcherStatus::EnumerationCompleted) {
                watcher.Stop();
            }
        } catch (winrt::hresult_error const&) {
        }
    }
}

std::vector<BluetoothEvent> BluetoothMonitor::take() {
    std::lock_guard guard(state_->lock);
    return std::exchange(state_->events, {});
}

std::vector<ConnectedBluetoothDevice> BluetoothMonitor::connectedDevices() const {
    std::unique_lock guard(state_->lock);
    std::map<std::wstring_view, ConnectedBluetoothDevice> devices;
    std::vector<std::wstring> refresh;
    const auto now = std::chrono::steady_clock::now();
    for (const auto& [id, endpoint] : state_->endpoints) {
        if (!endpoint.connected) continue;
        const auto container = std::wstring_view(endpoint.container.empty() ? id : endpoint.container);
        auto& device = devices[container];
        if (device.name.empty()) device.name = endpoint.name;
        device.audio = device.audio || endpoint.audio;
        if (endpoint.container.empty()) continue;
        auto& battery = state_->batteries[endpoint.container];
        device.battery = battery.value;
        if (!battery.pending && now - battery.requested >= std::chrono::seconds(30)) {
            battery.requested = now;
            battery.pending = true;
            refresh.push_back(endpoint.container);
        }
    }
    std::vector<ConnectedBluetoothDevice> result;
    result.reserve(devices.size());
    for (auto& entry : devices) {
        auto& device = entry.second;
        if (device.name.empty()) device.name = L"Bluetooth device";
        result.push_back(std::move(device));
    }
    guard.unlock();
    for (auto& container : refresh) refreshBattery(state_, std::move(container), now);
    std::ranges::stable_sort(result, {}, &ConnectedBluetoothDevice::name);
    return result;
}

}
