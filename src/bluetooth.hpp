#pragma once

#include <windows.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace zenith {

struct BluetoothEvent {
    std::wstring name;
    bool connected = false;
    bool audio = false;
    std::optional<unsigned> battery;
};

struct ConnectedBluetoothDevice {
    std::wstring name;
    bool audio = false;
    bool operator==(ConnectedBluetoothDevice const&) const = default;
};

// Watches paired Bluetooth devices and reports when one connects or disconnects. Devices that are
// already connected when watching starts stay silent.
class BluetoothMonitor {
public:
    static constexpr UINT changed = WM_APP + 12;

    BluetoothMonitor();
    ~BluetoothMonitor();
    BluetoothMonitor(BluetoothMonitor const&) = delete;
    BluetoothMonitor& operator=(BluetoothMonitor const&) = delete;

    void start(HWND window);
    void stop();
    // Connection changes since the last call, oldest first.
    std::vector<BluetoothEvent> take();
    std::vector<ConnectedBluetoothDevice> connectedDevices() const;

    struct State;

private:
    std::shared_ptr<State> state_;
};

}
