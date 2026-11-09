#include "power.hpp"

#include <windows.h>

namespace zenith {

PowerState readPower() {
    SYSTEM_POWER_STATUS status{};
    if (!GetSystemPowerStatus(&status)) {
        return {};
    }

    PowerState power;
    power.available = true;
    power.hasBattery = (status.BatteryFlag & 128) == 0 || status.BatteryFlag == 255;
    power.charging = status.BatteryFlag != 255 && (status.BatteryFlag & 8) != 0;
    if (status.ACLineStatus != 255) {
        power.pluggedIn = status.ACLineStatus == 1;
    }
    if (power.hasBattery && status.BatteryLifePercent <= 100) {
        power.percent = status.BatteryLifePercent;
    }
    if (power.hasBattery && status.BatteryLifeTime != MAXDWORD && power.pluggedIn == false) {
        power.remaining = std::chrono::seconds(status.BatteryLifeTime);
    }
    return power;
}

}
