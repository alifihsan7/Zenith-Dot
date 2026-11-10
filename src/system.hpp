#pragma once

#include <cstdint>

namespace zenith {

struct SystemStats {
    unsigned cpu = 0;
    unsigned memory = 0;

    bool operator==(SystemStats const&) const = default;
};

// CPU use is measured between two calls, so the first sample after a pause reads since the last one.
class SystemMonitor {
public:
    SystemStats sample();

private:
    uint64_t idle_ = 0;
    uint64_t total_ = 0;
};

}
