#pragma once

#include <windows.h>

#include <memory>
#include <string>

namespace zenith {

struct CaptureActivity {
    std::wstring microphone;
    std::wstring camera;
    bool operator==(CaptureActivity const&) const = default;
};

bool fullscreenOnPrimary(HWND overlay);
CaptureActivity readCaptureActivity();

class DesktopMonitor {
public:
    static constexpr UINT foregroundChanged = WM_APP + 4;
    static constexpr UINT captureChanged = WM_APP + 5;
    static constexpr UINT audioDeviceChanged = WM_APP + 6;
    static constexpr UINT volumeChanged = WM_APP + 7;
    static constexpr UINT brightnessChanged = WM_APP + 8;

    DesktopMonitor();
    ~DesktopMonitor();
    void start(HWND window);
    void stop();
    void refreshAudioDevice();
    bool isCurrentVolume(LPARAM generation) const;

private:
    struct State;
    std::unique_ptr<State> state_;
};

}
