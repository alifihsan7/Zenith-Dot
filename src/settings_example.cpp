#include "settings_example.hpp"

#include <cmath>

namespace zenith {
namespace {
using namespace std::chrono_literals;
}

SettingsExample::SettingsExample(SettingsTest test, Feature feature, Settings const& settings, bool animate)
    : test_(test), end_(start_ + 4s) {
    island_.setAnimate(animate);
    auto exampleSettings = settings;
    exampleSettings.quietMode = QuietMode::off;
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
    case SettingsTest::notification:
        island_.notify({IslandAlert::Kind::notification, L"Windows Clock", L"Timer finished", std::nullopt, 2, 2500ms});
        break;
    case SettingsTest::camera:
        if (settings.showCameraIndicator) {
            island_.setCaptureActivity({{}, L"Test", {}, false}, true);
        }
        break;
    case SettingsTest::microphone:
        if (settings.showMicrophoneIndicator) {
            island_.setCaptureActivity({L"Test", {}, {}, false}, true);
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
        if (feature == Feature::scratch) {
            scratch_.text = L"Example note";
            island_.setScratchpad(scratch_);
        }
        if (feature == Feature::system) {
            island_.setSystem({12, 45, 8, {{L'C', 3}, {L'D', 1}}});
            island_.setPower({true, true, true, true, 81, {}});
            island_.setAudioOutput(L"Headphones");
            island_.setAudioOutputs({{L"headphones", L"Headphones", true, true},
                                     {L"speakers", L"Speakers", false, false}});
            island_.setAppVolumes({{L"system", L"System sounds", 0.65f, false, false, {}}});
            island_.setNetwork({NetworkAccess::internet, NetworkType::wifi, L"Home Wi-Fi", 4});
            island_.setBluetoothDevices({{L"Headphones", true, 84}, {L"Wireless mouse", false, 63}});
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
    if (test_ == SettingsTest::waveform || test_ == SettingsTest::appearance) {
        Spectrum sample{};
        const auto seconds = std::chrono::duration<float>(elapsed).count();
        for (size_t i = 0; i < sample.size(); ++i) {
            const float phase = seconds * 11 - static_cast<float>(sample.size() - i - 1) * 0.48f;
            sample[i] = 0.18f + 0.70f * std::pow(std::max(0.0f, std::sin(phase)), 3.0f);
        }
        island_.setSpectrum(sample);
    }
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
    } else if (test_ == SettingsTest::copied || test_ == SettingsTest::camera ||
               test_ == SettingsTest::microphone || test_ == SettingsTest::notification) {
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
