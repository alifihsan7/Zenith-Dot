#include "settings_example.hpp"

namespace zenith {
namespace {
using namespace std::chrono_literals;
}

SettingsExample::SettingsExample(SettingsTest test, Feature feature, Settings const& settings, bool animate)
    : test_(test), end_(start_ + 4s) {
    island_.setAnimate(animate);
    auto exampleSettings = settings;
    if (test == SettingsTest::appearance) exampleSettings.enabledFeatures |= 1u << static_cast<unsigned>(Feature::media);
    island_.setPreferences(exampleSettings);
    media_.hasSession = media_.playing = true;
    media_.title = L"Song Title";
    media_.artist = L"Artist Name";
    media_.duration = 180;
    media_.position = 60;
    media_.sampledAt = start_;

    switch (test) {
    case SettingsTest::appearance:
        end_ = start_ + 5s;
        break;
    case SettingsTest::waveform:
    case SettingsTest::lyrics:
        if (test == SettingsTest::lyrics) {
            lyrics_.status = LyricsStatus::ready;
            lyrics_.lines.push_back({0, L"Example lyric line"});
            island_.setLyrics(lyrics_);
            island_.lyricsOpen_ = true;
        }
        island_.setMedia(media_);
        break;
    case SettingsTest::pause:
        end_ = start_ + std::chrono::seconds(std::max(3u, settings.pauseSeconds() + 2));
        island_.setMedia(media_);
        playing_ = true;
        break;
    case SettingsTest::focus:
    case SettingsTest::resting:
    case SettingsTest::cycle:
        island_.selectFocusPhase(test == SettingsTest::resting);
        if (test == SettingsTest::cycle) {
            island_.focus_.configure(1);
            island_.toggleFocus(start_);
        }
        showFeature(Feature::timers, start_);
        break;
    case SettingsTest::copied:
        island_.notify({IslandAlert::Kind::copy, L"Copied", settings.showClipboardPreviews ? L"Example text" : L""});
        break;
    case SettingsTest::camera:
        if (settings.showCameraIndicator) {
            island_.setCaptureColors({}, 0x30d158);
            island_.notify({IslandAlert::Kind::camera, L"Camera", L"Test", 0x30d158});
        }
        break;
    case SettingsTest::microphone:
        if (settings.showMicrophoneIndicator) {
            island_.setCaptureColors(0xff9f0a, {});
            island_.notify({IslandAlert::Kind::microphone, L"Microphone", L"Test", 0xff9f0a});
        }
        break;
    case SettingsTest::copiedText:
    case SettingsTest::screenshot:
        if (test == SettingsTest::copiedText && settings.autoAddText) {
            island_.setDropItems({{L"Example text", true, nullptr}});
        } else if (test == SettingsTest::screenshot && settings.autoAddScreenshots) {
            auto image = std::make_shared<DropThumbnail>();
            image->width = image->height = 12;
            image->pixels.resize(12 * 12 * 4);
            for (size_t offset = 0; offset < image->pixels.size(); offset += 4) {
                image->pixels[offset] = 0xff;
                image->pixels[offset + 1] = 0xca;
                image->pixels[offset + 2] = 0x91;
                image->pixels[offset + 3] = 0xff;
            }
            island_.setDropItems({{L"Example screenshot.png", false, std::move(image)}});
        }
        showFeature(Feature::drop, start_);
        break;
    case SettingsTest::feature:
    case SettingsTest::order:
        if (feature == Feature::media) island_.setMedia(media_);
        if (feature == Feature::scratch) island_.setScratchpad({L"Example note"});
        if (feature == Feature::system) {
            island_.setSystem({12, 45});
            island_.setAudioOutput(L"Speakers");
        }
        showFeature(feature, start_);
        break;
    }
}

void SettingsExample::showFeature(Feature feature, Clock::time_point now) {
    island_.showScreen(feature, now);
    island_.setOpen(true, now);
}

void SettingsExample::draw(HWND window, Clock::time_point now) {
    const auto elapsed = now - start_;
    if (test_ == SettingsTest::appearance) {
        if (!playing_ && elapsed >= 350ms) {
            island_.setMedia(media_);
            playing_ = true;
        }
        if (!panelOpened_ && elapsed >= 1550ms) {
            showFeature(Feature::media, now);
            panelOpened_ = true;
        }
        if (!panelClosed_ && elapsed >= 3700ms) {
            island_.closePanel();
            panelClosed_ = true;
        }
    } else if (test_ == SettingsTest::pause && playing_ && elapsed >= 650ms) {
        media_.playing = false;
        island_.setMedia(media_);
        playing_ = false;
    } else if (test_ == SettingsTest::cycle && island_.focus_.expire(now)) {
        island_.finishFocus(now);
    } else if (test_ == SettingsTest::copied || test_ == SettingsTest::camera || test_ == SettingsTest::microphone) {
        island_.tick();
    }
    island_.draw(window);
}

unsigned SettingsExample::frameInterval(Island::Clock::time_point now) const {
    auto next = end_;
    if (test_ == SettingsTest::appearance) {
        if (!playing_) next = start_ + 350ms;
        else if (!panelOpened_) next = start_ + 1550ms;
        else if (!panelClosed_) next = start_ + 3700ms;
    } else if (test_ == SettingsTest::pause && playing_) {
        next = start_ + 650ms;
    }
    const auto remaining = static_cast<unsigned>(std::max(1ms, std::chrono::ceil<std::chrono::milliseconds>(next - now)).count());
    const auto motion = island_.frameInterval();
    return motion ? std::min(remaining, motion) : remaining;
}

}
