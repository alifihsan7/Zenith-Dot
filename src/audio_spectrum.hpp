#pragma once

#include <array>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace zenith {

using Spectrum = std::array<float, 20>;

class AudioSpectrum {
public:
    void select(std::wstring_view app);
    Spectrum levels() const;

private:
    void listen(std::stop_token stop, std::wstring const& app);

    std::wstring app_;
    mutable std::mutex mutex_;
    Spectrum levels_{};
    std::jthread worker_;
};

}
