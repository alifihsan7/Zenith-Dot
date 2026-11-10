#pragma once

#include <algorithm>
#include <chrono>
#include <optional>

namespace zenith {

class Countdown {
public:
    using Clock = std::chrono::steady_clock;

    explicit Countdown(int seconds) : duration_(seconds), left_(duration_) {}

    bool running() const { return end_.has_value(); }
    bool paused() const { return !running() && left_ > Clock::duration::zero() && left_ < duration_; }
    bool done() const { return !running() && left_ == Clock::duration::zero(); }
    int duration() const { return static_cast<int>(duration_.count()); }

    int seconds(Clock::time_point now) const {
        return static_cast<int>(std::chrono::ceil<std::chrono::seconds>(remaining(now)).count());
    }

    void configure(int seconds) {
        duration_ = std::chrono::seconds(seconds);
        reset();
    }

    void reset() {
        left_ = duration_;
        end_.reset();
    }

    void toggle(Clock::time_point now) {
        if (end_) {
            if (now >= *end_) return;
            left_ = remaining(now);
            end_.reset();
        } else {
            if (done()) left_ = duration_;
            end_ = now + left_;
        }
    }

    bool expire(Clock::time_point now) {
        if (!end_ || now < *end_) return false;
        left_ = {};
        end_.reset();
        return true;
    }

private:
    Clock::duration remaining(Clock::time_point now) const {
        return end_ ? std::max(Clock::duration::zero(), *end_ - now) : left_;
    }

    std::chrono::seconds duration_;
    Clock::duration left_;
    std::optional<Clock::time_point> end_;
};

}
