#include "media.hpp"

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Storage.Streams.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <utility>

namespace zenith {
namespace {

using namespace winrt::Windows::Media::Control;
using Status = GlobalSystemMediaTransportControlsSessionPlaybackStatus;
using Session = GlobalSystemMediaTransportControlsSession;

enum Update : WPARAM { sessions, playback, timeline, metadata };

bool isPlaying(Session const& session) {
    try {
        const auto info = session.GetPlaybackInfo();
        return info && info.PlaybackStatus() == Status::Playing;
    } catch (winrt::hresult_error const&) {
        return false;
    }
}

auto signal(HWND window, Update update) {
    return [window, update](auto const&, auto const&) {
        PostMessageW(window, MediaMonitor::notification, update, 0);
    };
}

unsigned fromHsl(float hue, float saturation, float lightness) {
    const float chroma = saturation * std::min(lightness, 1.0f - lightness);
    const auto level = [=](float offset) {
        const float k = std::fmod(offset + hue / 30.0f, 12.0f);
        const float value = lightness - chroma * std::max(-1.0f, std::min({k - 3.0f, 9.0f - k, 1.0f}));
        return static_cast<unsigned>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
    };
    return (level(0) << 16) | (level(8) << 8) | level(4);
}

// Picks the cover's most vivid hue, then clamps it so it stays readable on the black island.
// Pixels vote into twelve hue buckets, weighted by saturation and distance from black or white.
CoverTheme coverTheme(std::span<const uint8_t> bgra) {
    struct Bucket { float weight = 0, x = 0, y = 0, saturation = 0, lightness = 0; };
    constexpr float degrees = 3.14159265f / 180.0f;
    std::array<Bucket, 12> buckets{};
    for (size_t pixel = 0; pixel + 3 < bgra.size(); pixel += 4) {
        const float b = bgra[pixel] / 255.0f;
        const float g = bgra[pixel + 1] / 255.0f;
        const float r = bgra[pixel + 2] / 255.0f;
        const float high = std::max({r, g, b});
        const float delta = high - std::min({r, g, b});
        if (delta <= 0) continue;
        const float lightness = high - delta / 2;
        const float saturation = delta / (1.0f - std::abs(2.0f * lightness - 1.0f));
        if (saturation < 0.12f) continue;
        float hue = high == r ? std::fmod((g - b) / delta, 6.0f) :
            (high == g ? (b - r) / delta + 2 : (r - g) / delta + 4);
        hue *= 60;
        if (hue < 0) hue += 360;
        const float score = saturation * (1.0f - std::abs(lightness - 0.5f) * 1.4f);
        auto& bucket = buckets[static_cast<size_t>(hue / 30.0f) % buckets.size()];
        bucket.weight += score;
        bucket.x += score * std::cos(hue * degrees);
        bucket.y += score * std::sin(hue * degrees);
        bucket.saturation += score * saturation;
        bucket.lightness += score * lightness;
    }
    const auto& best = *std::ranges::max_element(buckets, {}, &Bucket::weight);
    // A greyscale or nearly colourless cover keeps the default colours.
    if (best.weight < static_cast<float>(bgra.size() / 4) * 0.02f) return {};
    float hue = std::atan2(best.y, best.x) / degrees;
    if (hue < 0) hue += 360;
    const float saturation = std::clamp(best.saturation / best.weight, 0.45f, 0.9f);
    const float lightness = std::clamp(best.lightness / best.weight, 0.68f, 0.78f);
    return {fromHsl(hue, saturation, lightness), fromHsl(hue, saturation * 0.55f, 0.09f)};
}

void sendMediaKey(MediaCommand command) {
    WORD key = VK_MEDIA_PLAY_PAUSE;
    if (command == MediaCommand::previous) key = VK_MEDIA_PREV_TRACK;
    if (command == MediaCommand::next) key = VK_MEDIA_NEXT_TRACK;

    INPUT input[2]{};
    input[0].type = input[1].type = INPUT_KEYBOARD;
    input[0].ki.wVk = input[1].ki.wVk = key;
    input[0].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
    input[1].ki.dwFlags = KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP;
    SendInput(2, input, sizeof(INPUT));
}

}

double MediaState::positionAt(std::chrono::steady_clock::time_point now) const {
    const double elapsed = playing ? std::chrono::duration<double>(now - sampledAt).count() * rate : 0;
    return std::clamp(position + elapsed, 0.0, duration);
}

MediaMonitor::MediaMonitor(HWND window, std::function<void(MediaState const&)> changed)
    : window_(window), changed_(std::move(changed)) {}

winrt::Windows::Foundation::IAsyncAction MediaMonitor::start() {
    const auto lifetime = shared_from_this();
    try {
        const auto manager = co_await Manager::RequestAsync();
        if (stopped_) co_return;

        manager_ = manager;
        sessionsChanged_ = manager_.SessionsChanged(winrt::auto_revoke, signal(window_, sessions));
        currentChanged_ = manager_.CurrentSessionChanged(winrt::auto_revoke, signal(window_, playback));
        refresh(sessions);
    } catch (winrt::hresult_error const&) {
        OutputDebugStringW(L"Zenith Dot could not start Windows media monitoring.\n");
    }
}

void MediaMonitor::stop() {
    stopped_ = true;
    sessionsChanged_.revoke();
    currentChanged_.revoke();
    metadataChanged_.revoke();
    timelineChanged_.revoke();
    sessions_.clear();
    selected_ = nullptr;
    manager_ = nullptr;
    changed_ = nullptr;
}

void MediaMonitor::updateSessions() {
    std::vector<WatchedSession> updated;
    for (const auto& session : manager_.GetSessions()) {
        try {
            updated.push_back({session, session.PlaybackInfoChanged(winrt::auto_revoke, signal(window_, playback))});
        } catch (winrt::hresult_error const&) {
            // A session can close between enumeration and subscription.
        }
    }
    sessions_ = std::move(updated);
}

bool MediaMonitor::selectSession() {
    const auto current = manager_.GetCurrentSession();
    auto next = current;
    if (!next || !isPlaying(next)) {
        const auto playing = std::ranges::find_if(sessions_, [](WatchedSession const& entry) {
            return isPlaying(entry.session);
        });
        if (playing != sessions_.end()) next = playing->session;
    }
    if (selected_ == next) return false;

    metadataChanged_.revoke();
    timelineChanged_.revoke();
    selected_ = next;
    state_ = {};
    timelineUpdated_ = {};
    if (selected_) {
        try {
            metadataChanged_ = selected_.MediaPropertiesChanged(winrt::auto_revoke, signal(window_, metadata));
            timelineChanged_ = selected_.TimelinePropertiesChanged(winrt::auto_revoke, signal(window_, timeline));
        } catch (winrt::hresult_error const&) {
            // A queued session change will replace a session that just closed.
        }
    }
    return true;
}

void MediaMonitor::updateState() {
    const auto now = std::chrono::steady_clock::now();
    const auto previousPosition = state_.positionAt(now);
    state_.hasSession = static_cast<bool>(selected_);
    state_.playing = false;
    state_.sampledAt = now;
    if (selected_) {
        try {
            const auto info = selected_.GetPlaybackInfo();
            const auto properties = selected_.GetTimelineProperties();
            state_.playing = info && info.PlaybackStatus() == Status::Playing;
            const auto rate = info ? info.PlaybackRate() : nullptr;
            state_.rate = rate ? rate.Value() : 1.0;
            if (!std::isfinite(state_.rate)) state_.rate = 1.0;
            if (properties) {
                const auto start = properties.StartTime();
                const auto updated = properties.LastUpdatedTime();
                state_.duration = std::max(0.0, std::chrono::duration<double>(properties.EndTime() - start).count());
                state_.position = std::chrono::duration<double>(properties.Position() - start).count();
                if (timelineUpdated_ == updated && updated.time_since_epoch().count() != 0) {
                    state_.position = previousPosition;
                } else if (state_.playing && updated.time_since_epoch().count() != 0) {
                    const auto age = winrt::clock::now() - updated;
                    state_.position += std::max(0.0, std::chrono::duration<double>(age).count()) * state_.rate;
                }
                timelineUpdated_ = updated;
                state_.position = std::clamp(state_.position, 0.0, state_.duration);
            } else {
                state_.position = previousPosition;
            }
        } catch (winrt::hresult_error const&) {
            state_.playing = false;
            state_.position = previousPosition;
        }
    }
    changed_(state_);
}

void MediaMonitor::refresh(WPARAM reason) {
    if (stopped_ || !manager_) return;
    try {
        if (reason == sessions) updateSessions();
        const bool changedSession = (reason == sessions || reason == playback) && selectSession();
        if (reason != metadata || changedSession) updateState();
        if (changedSession || reason == metadata) requestMetadata();
    } catch (winrt::hresult_error const&) {
        OutputDebugStringW(L"Zenith Dot could not refresh the current Windows media session.\n");
    }
}

void MediaMonitor::requestMetadata() {
    ++metadataRevision_;
    if (selected_ && !readingMetadata_) readMetadata();
}

winrt::Windows::Foundation::IAsyncAction MediaMonitor::readMetadata() {
    const auto lifetime = shared_from_this();
    readingMetadata_ = true;
    while (!stopped_ && selected_) {
        const auto session = selected_;
        const auto revision = metadataRevision_;
        try {
            const auto properties = co_await session.TryGetMediaPropertiesAsync();
            const auto current = [&] { return !stopped_ && session == selected_ && revision == metadataRevision_; };
            if (properties && current()) {
                state_.title = properties.Title().c_str();
                state_.artist = properties.Artist().c_str();
                state_.album = properties.AlbumTitle().c_str();
                changed_(state_);

                // The text shows right away; the colours follow once the cover has been decoded.
                CoverTheme theme;
                try {
                    if (const auto thumbnail = properties.Thumbnail()) {
                        using namespace winrt::Windows::Graphics::Imaging;
                        const auto stream = co_await thumbnail.OpenReadAsync();
                        const auto decoder = co_await BitmapDecoder::CreateAsync(stream);
                        const BitmapTransform transform;
                        transform.ScaledWidth(16);
                        transform.ScaledHeight(16);
                        transform.InterpolationMode(BitmapInterpolationMode::Fant);
                        const auto pixels = co_await decoder.GetPixelDataAsync(BitmapPixelFormat::Bgra8,
                            BitmapAlphaMode::Ignore, transform, ExifOrientationMode::IgnoreExifOrientation,
                            ColorManagementMode::DoNotColorManage);
                        const auto bytes = pixels.DetachPixelData();
                        theme = coverTheme({bytes.data(), bytes.size()});
                    }
                } catch (winrt::hresult_error const&) {
                    // A missing or unreadable cover keeps the default colours.
                }
                if (current() && state_.theme != theme) {
                    state_.theme = theme;
                    changed_(state_);
                }
            }
        } catch (winrt::hresult_error const&) {
            // Some players stop exposing metadata as their session closes.
        }
        if (revision == metadataRevision_) break;
    }
    readingMetadata_ = false;
}

winrt::Windows::Foundation::IAsyncAction MediaMonitor::control(MediaCommand command) {
    const auto lifetime = shared_from_this();
    const auto session = selected_;
    try {
        if (session) {
            bool handled = false;
            switch (command) {
            case MediaCommand::previous: handled = co_await session.TrySkipPreviousAsync(); break;
            case MediaCommand::toggle: handled = co_await session.TryTogglePlayPauseAsync(); break;
            case MediaCommand::next: handled = co_await session.TrySkipNextAsync(); break;
            }
            if (handled) co_return;
        }
        if (!stopped_ && session == selected_ && (!session || manager_.GetCurrentSession() == session)) {
            sendMediaKey(command);
        }
    } catch (winrt::hresult_error const&) {
        // A closed session can no longer be safely targeted.
    }
}

}
