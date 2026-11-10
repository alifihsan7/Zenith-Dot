#pragma once

#include <windows.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace zenith {

struct CaptureActivity {
    std::wstring microphone;
    std::wstring camera;
    bool operator==(CaptureActivity const&) const = default;
};

struct AudioOutput {
    std::wstring id;
    std::wstring name;
    bool active = false;
};

bool fullscreenOnPrimary(HWND overlay);
bool activateMediaWindow(HWND overlay, std::wstring const& appId);
CaptureActivity readCaptureActivity();

class DesktopMonitor {
public:
    static constexpr UINT foregroundChanged = WM_APP + 4;
    static constexpr UINT captureChanged = WM_APP + 5;
    static constexpr UINT audioDeviceChanged = WM_APP + 6;
    static constexpr UINT volumeChanged = WM_APP + 7;
    static constexpr UINT brightnessChanged = WM_APP + 8;
    static constexpr UINT notificationModeChanged = WM_APP + 10;

    DesktopMonitor();
    ~DesktopMonitor();
    void start(HWND window);
    void stop();
    void refreshAudioDevice();
    std::wstring const& audioOutput() const;
    std::vector<AudioOutput> audioOutputs() const;
    bool selectAudioOutput(std::wstring const& id);
    CaptureActivity captureActivity() const;
    std::optional<bool> notificationsQuiet() const;
    bool isCurrentVolume(LPARAM generation) const;

private:
    struct State;
    std::unique_ptr<State> state_;
};

}
