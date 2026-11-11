#pragma once

#include "island.hpp"

namespace zenith {

class SettingsExample {
public:
    SettingsExample(SettingsTest test, Feature feature, Settings const& settings, bool animate);
    bool finished(Island::Clock::time_point now) const { return now >= end_; }
    void draw(HWND window, Island::Clock::time_point now);
    unsigned frameInterval(Island::Clock::time_point now) const;

private:
    void showFeature(Feature feature, Island::Clock::time_point now);

    using Clock = Island::Clock;
    Island island_;
    MediaState media_;
    LyricsView lyrics_;
    ScratchpadView scratch_;
    SettingsTest test_;
    Clock::time_point start_ = Clock::now();
    Clock::time_point end_;
    bool playing_ = false;
    bool panelOpened_ = false;
    bool panelClosed_ = false;
};

}
