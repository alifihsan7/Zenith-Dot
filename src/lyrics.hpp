#pragma once

#include "media.hpp"

namespace zenith {

struct LyricLine {
    double seconds;
    std::wstring text;
};

enum class LyricsStatus { idle, loading, ready, plain, unavailable, error };

struct LyricsView {
    LyricsStatus status = LyricsStatus::idle;
    std::vector<LyricLine> lines;
    std::wstring plain;
    unsigned revision = 0;
};

class Lyrics : public std::enable_shared_from_this<Lyrics> {
public:
    explicit Lyrics(std::function<void()> changed);
    void update(MediaState const& media, bool open);
    void retry();
    void stop();
    LyricsView const& view() const { return view_; }

private:
    using Action = winrt::Windows::Foundation::IAsyncAction;
    Action lookup(unsigned revision, std::wstring title, std::wstring artist, std::wstring album, unsigned duration);
    void cancel();

    std::function<void()> changed_;
    LyricsView view_;
    std::wstring appId_, title_, artist_, album_;
    unsigned duration_ = 0;
    unsigned revision_ = 0;
    std::chrono::steady_clock::time_point retryAfter_{};
    bool open_ = false;
    bool stopped_ = false;
    Action request_{nullptr};
};

}
