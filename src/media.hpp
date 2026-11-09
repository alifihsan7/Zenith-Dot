#pragma once

#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace zenith {

enum class MediaCommand { previous, toggle, next };

struct MediaState {
    bool hasSession = false;
    bool playing = false;
    std::wstring title;
    std::wstring artist;
    std::wstring album;
    double duration = 0;
    double position = 0;
    double rate = 1;
    std::chrono::steady_clock::time_point sampledAt{};

    double positionAt(std::chrono::steady_clock::time_point now) const;
};

class MediaMonitor : public std::enable_shared_from_this<MediaMonitor> {
public:
    static constexpr UINT notification = WM_APP + 1;

    MediaMonitor(HWND window, std::function<void(MediaState const&)> changed);
    winrt::Windows::Foundation::IAsyncAction start();
    void stop();
    void refresh(WPARAM reason);
    winrt::Windows::Foundation::IAsyncAction control(MediaCommand command);

private:
    using Manager = winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager;
    using Session = winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession;

    struct WatchedSession {
        Session session;
        Session::PlaybackInfoChanged_revoker playback;
    };

    void updateSessions();
    bool selectSession();
    void updateState();
    void requestMetadata();
    winrt::Windows::Foundation::IAsyncAction readMetadata();

    HWND window_;
    std::function<void(MediaState const&)> changed_;
    Manager manager_{nullptr};
    Manager::SessionsChanged_revoker sessionsChanged_;
    Manager::CurrentSessionChanged_revoker currentChanged_;
    std::vector<WatchedSession> sessions_;
    Session selected_{nullptr};
    Session::MediaPropertiesChanged_revoker metadataChanged_;
    Session::TimelinePropertiesChanged_revoker timelineChanged_;
    MediaState state_;
    winrt::Windows::Foundation::DateTime timelineUpdated_{};
    unsigned metadataRevision_ = 0;
    bool readingMetadata_ = false;
    bool stopped_ = false;
};

}
