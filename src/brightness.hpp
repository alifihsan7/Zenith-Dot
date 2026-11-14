#pragma once

#include <windows.h>

#include <memory>
#include <optional>

namespace zenith {

// Brightness of the display the island sits on: DDC/CI for an external monitor, WMI for a
// built-in panel. Monitor calls can take tens of milliseconds, so they run on a worker thread
// that posts `changed` when the level moves.
class BrightnessControl {
public:
    static constexpr UINT changed = WM_APP + 25;

    BrightnessControl();
    ~BrightnessControl();
    BrightnessControl(BrightnessControl const&) = delete;
    BrightnessControl& operator=(BrightnessControl const&) = delete;

    void start(HWND window);
    void stop();
    // Reads the level again, for changes made with the monitor's buttons or in Windows.
    void refresh();
    // 0..1, or empty when the display offers no brightness control.
    std::optional<float> level() const;
    // Applies the latest value only; a drag sends many and the display gets the last one.
    void set(float value);

    struct State;

private:
    std::shared_ptr<State> state_;
};

}
