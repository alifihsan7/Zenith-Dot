#pragma once

#include <chrono>
#include <optional>

namespace zenith {

struct PowerState {
    bool available = false;
    bool hasBattery = false;
    bool charging = false;
    std::optional<bool> pluggedIn;
    std::optional<unsigned> percent;
    std::optional<std::chrono::seconds> remaining;

    bool operator==(PowerState const&) const = default;
};

PowerState readPower();

}
