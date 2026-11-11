#pragma once

#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace zenith {

enum class MediaCommand { previous, toggle, next };

// Colours taken from the current cover (0xRRGGBB). The defaults apply when there is no usable cover.
struct CoverTheme {
    unsigned accent = 0x91caff;
    unsigned tint = 0x0c1422;

    bool operator==(CoverTheme const&) const = default;
};

// The current cover, center-cropped to a square, as premultiplied BGRA.
struct Cover {
    unsigned size = 0;
    std::vector<uint8_t> pixels;
};

// A Windows media app.
struct MediaSource {
    std::wstring appId;
    bool playing = false;
};

struct MediaState {
    bool hasSession = false;
    bool playing = false;
    std::wstring appId;
    std::wstring title;
    std::wstring artist;
    std::wstring album;
    CoverTheme theme;
    std::shared_ptr<const Cover> cover;
    std::vector<MediaSource> sources;
    bool canSeek = false;
    bool canPrevious = false;
    bool canNext = false;
    double duration = 0;
    double position = 0;
    double rate = 1;
    std::chrono::steady_clock::time_point sampledAt{};

    double positionAt(std::chrono::steady_clock::time_point now) const;
};

class MediaMonitor : public std::enable_shared_from_this<MediaMonitor> {
public:
    static constexpr UINT notification = WM_APP + 1;
    static constexpr UINT_PTR artworkTimer = 4;

    MediaMonitor(HWND window, std::function<void(MediaState const&)> changed);
    winrt::Windows::Foundation::IAsyncAction start();
    void stop();
    void refresh(WPARAM reason);
    void retryArtwork();
    winrt::Windows::Foundation::IAsyncAction control(MediaCommand command);
    winrt::Windows::Foundation::IAsyncAction seek(double seconds);
    // Shows and controls this app's session until the user switches or the session closes.
    void select(std::wstring const& appId);

private:
    using Manager = winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager;
    using Session = winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession;

    struct WatchedSession {
        Session session;
        Session::PlaybackInfoChanged_revoker playback;
    };
    struct Artwork {
        std::wstring appId, title, artist, album;
        CoverTheme theme;
        std::shared_ptr<const Cover> cover;
        bool matches(MediaState const& state) const;
    };

    void updateSessions();
    bool selectSession();
    void updateState();
    void requestMetadata();
    winrt::Windows::Foundation::IAsyncAction readMetadata();
    void restoreArtwork();
    void rememberArtwork();

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
    std::wstring preferred_;
    winrt::Windows::Foundation::TimeSpan timelineStart_{};
    winrt::Windows::Foundation::DateTime timelineUpdated_{};
    unsigned metadataRevision_ = 0;
    unsigned artworkRetries_ = 0;
    std::vector<Artwork> artwork_;
    bool readingMetadata_ = false;
    bool stopped_ = false;
};

}
