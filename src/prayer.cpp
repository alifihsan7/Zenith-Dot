#include "prayer.hpp"

#include <cmath>
#include <cwchar>
#include <numbers>

namespace zenith {
namespace {

struct City {
    wchar_t const* name;
    double latitude;
    double longitude;
    int utcOffset;
};

constexpr City cities[]{
    {L"Jakarta", -6.1754, 106.8272, 7},
    {L"Makassar", -5.1477, 119.4327, 8},
    {L"Jayapura", -2.5916, 140.6690, 9},
};

constexpr double radians = std::numbers::pi / 180;

double wrap(double value, double period) {
    return value - period * std::floor(value / period);
}

struct Sun {
    double declination;
    double noon;

    double hourAngle(double latitude, double altitude) const {
        const double cosine = (std::sin(altitude) - std::sin(latitude) * std::sin(declination)) /
            (std::cos(latitude) * std::cos(declination));
        return std::acos(cosine) / (15 * radians);
    }
};

// Solar position and hour-angle equations from https://praytimes.org/docs/calculation.
Sun solarPosition(double days, City const& city) {
    const double anomaly = (357.529 + 0.98560028 * days) * radians;
    const double meanLongitude = wrap(280.459 + 0.98564736 * days, 360);
    const double longitude = (meanLongitude + 1.915 * std::sin(anomaly) + 0.020 * std::sin(2 * anomaly)) * radians;
    const double tilt = (23.439 - 0.00000036 * days) * radians;
    const double rightAscension = std::atan2(std::cos(tilt) * std::sin(longitude), std::cos(longitude)) / (15 * radians);
    const double equation = wrap(meanLongitude / 15 - rightAscension + 12, 24) - 12;
    return {std::asin(std::sin(tilt) * std::sin(longitude)), 12 + city.utcOffset - city.longitude / 15 - equation};
}

}

std::chrono::sys_days prayerDate(unsigned city, std::chrono::system_clock::time_point now) {
    if (!city || city > std::size(cities)) return {};
    return std::chrono::floor<std::chrono::days>(now + std::chrono::hours(cities[city - 1].utcOffset));
}

std::optional<PrayerSchedule> prayerSchedule(unsigned index, std::chrono::sys_days date) {
    using namespace std::chrono;
    if (!index || index > std::size(cities)) return std::nullopt;
    const auto& city = cities[index - 1];
    const double latitude = city.latitude * radians;
    const double days = (date - sys_days{year{2000}/January/1}).count() - city.utcOffset / 24.0;
    const auto calculate = [&](size_t prayer, double estimate) {
        const auto sun = solarPosition(days + (estimate - 12) / 24, city);
        switch (prayer) {
        case 0: return sun.noon - sun.hourAngle(latitude, -20 * radians);
        case 1: return sun.noon;
        case 2: {
            const double altitude = std::atan(1 / (1 + std::tan(std::abs(latitude - sun.declination))));
            return sun.noon + sun.hourAngle(latitude, altitude);
        }
        case 3: return sun.noon + sun.hourAngle(latitude, -0.833 * radians);
        default: return sun.noon + sun.hourAngle(latitude, -18 * radians);
        }
    };
    constexpr std::array guesses{5, 12, 15, 18, 19};
    PrayerSchedule schedule{city.name, date, {}, city.utcOffset};
    for (size_t i = 0; i < schedule.times.size(); ++i) {
        const double hours = calculate(i, calculate(i, guesses[i]));
        schedule.times[i] = static_cast<int>(std::ceil(60 * hours)) + 1;
    }
    return schedule;
}

std::wstring prayerTimeText(int minute) {
    wchar_t result[6]{};
    swprintf_s(result, L"%02d:%02d", minute / 60, minute % 60);
    return result;
}

}
