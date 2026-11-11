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
    std::wstring screen;
    bool recording = false;
};

struct AudioOutput {
    std::wstring id;
    std::wstring name;
    bool active = false;
};

// One app's audio on the default output, merged across its sessions (as in the Windows volume mixer).
struct AppVolume {
    std::wstring id;     // the executable path, or "system" for system sounds
    std::wstring name;
    float volume = 1;    // 0..1
    bool muted = false;
    bool active = false; // currently playing
    bool operator==(AppVolume const&) const = default;
};

bool fullscreenOnPrimary(HWND overlay);
bool activateMediaWindow(HWND overlay, std::wstring const& appId);
CaptureActivity readCaptureActivity();

class DesktopMonitor {
public:
    static constexpr UINT foregroundChanged = WM_APP + 4;
    static constexpr UINT captureChanged = WM_APP + 5;
    static constexpr UINT audioDeviceChanged = WM_APP + 6;
    static constexpr UINT notificationModeChanged = WM_APP + 10;

    DesktopMonitor();
    ~DesktopMonitor();
    void start(HWND window);
    void stop();
    void refreshAudioDevice();
    std::wstring const& audioOutput() const;
    std::vector<AudioOutput> audioOutputs() const;
    bool selectAudioOutput(std::wstring const& id);
    std::vector<AppVolume> appVolumes() const;
    bool setAppVolume(std::wstring const& id, std::optional<float> volume, std::optional<bool> muted);
    CaptureActivity captureActivity() const;
    std::optional<bool> notificationsQuiet() const;

private:
    struct State;
    std::unique_ptr<State> state_;
};

}
