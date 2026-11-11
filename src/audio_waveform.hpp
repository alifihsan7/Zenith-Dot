#pragma once

#include <array>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace zenith {

using Waveform = std::array<float, 24>;

class AudioWaveform {
public:
    void select(std::wstring_view app);
    Waveform samples() const;

private:
    void listen(std::stop_token stop, std::wstring const& app);

    std::wstring app_;
    mutable std::mutex mutex_;
    Waveform samples_{};
    std::jthread worker_;
};

}
