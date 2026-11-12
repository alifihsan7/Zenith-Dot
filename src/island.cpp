#include "island.hpp"

#include <shobjidl.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <iterator>

namespace zenith {
namespace {

using namespace std::chrono_literals;

// Geometry in DIPs, taken from preview V7.
constexpr float pillTop = 8;
constexpr float pillWidth = 147;
constexpr float pillHeight = 40;
constexpr float pausedPillWidth = 46;
constexpr float pausedPillHeight = 32;
constexpr float pillTrackLeft = 32;
constexpr float pillTrackRight = 38;
constexpr float maxPillLyricWidth = 720;
constexpr float maxPillLyricHeight = 160;
constexpr float pillLyricIconSpace = 24;
constexpr float pillLyricTravel = 4;
constexpr float pillCenterY = pillTop + pillHeight / 2;
constexpr float pillLeft = -pillWidth / 2;
constexpr float dotSize = 12;
constexpr float activityDotSize = pausedPillHeight;
constexpr float privacyIconPitch = 18;
constexpr float privacyBadgeHeight = 25;
constexpr std::array<size_t, 3> privacyIconOrder{1, 2, 0};
constexpr wchar_t const* privacyLabels[]{L"Screen sharing", L"Camera", L"Microphone"};
constexpr auto dotHoverTime = 180ms;
constexpr float activityDotGap = 8;
constexpr auto activitySwapTime = 440ms;
constexpr auto activityHandoffTime = 980ms;
constexpr auto idleBeforePausedMedia = 110ms;
constexpr auto activityDotExitTime = 240ms;
constexpr auto countdownCompletionMotion = 280ms;
constexpr auto navSelectionTime = 240ms;
constexpr auto timerDigitMotion = 170ms;
constexpr float timerDragStep = 12;
constexpr float activitySwapDrop = 31;
constexpr float activitySwapLift = 7;
constexpr float panelWidth = 369;
constexpr float mediaPanelWidth = 344;
constexpr float systemPanelWidth = 530;
constexpr float systemStatsBottom = 75;
constexpr float storageCardsTop = systemStatsBottom + 21;
constexpr size_t storageColumns = 4;
constexpr float storageCardPitch = 58;
constexpr float batteryCardHeight = 41;
constexpr float connectionTop = batteryCardHeight + 8;
constexpr float systemRightGap = 19;
constexpr float systemColumnWidth = (systemPanelWidth - 28 - systemRightGap) / 2;
constexpr float systemAudioTop = 20;
constexpr float systemAudioHeight = 29;
constexpr float systemAudioMenuTop = systemAudioTop + systemAudioHeight + 4;
constexpr float pickerRowHeight = 25;
constexpr float pickerInset = 4;
constexpr float prayerPickerTop = 95;
constexpr float prayerPickerHeight = 24;
constexpr float prayerMenuTop = prayerPickerTop + prayerPickerHeight + 4;
constexpr float prayerPanelHeight = 176;
constexpr float prayerPanelWidth = 570;
constexpr float prayerInfoWidth = 176;
constexpr float appRowsTop = 73;
constexpr float appRowHeight = 28;
constexpr float bluetoothRowsTop = 73;
constexpr float bluetoothRowHeight = 33;
constexpr size_t visibleAppRows = 3;
constexpr size_t visibleAudioOutputs = 3;
constexpr size_t visibleBluetoothRows = 3;
constexpr float mediaPanelHeight = 239;
constexpr float mediaIdleHeight = 88;
constexpr float mediaCoverSize = 80;
constexpr float mediaCoverTop = 12;
constexpr float mediaTitleSize = 19;
constexpr float mediaTitleHeight = 26;
constexpr float mediaTitleTop = mediaCoverTop;
constexpr float mediaArtistHeight = 16.8f;
constexpr float mediaControlsCenterY = 159;
constexpr float mediaActionHeight = 32;
constexpr float mediaActionsTop = mediaControlsCenterY - mediaActionHeight / 2;
constexpr float sourceMenuTop = mediaActionsTop + mediaActionHeight + 4;
constexpr size_t sourceColumns = 8;
constexpr size_t visibleSources = 2 * sourceColumns;
constexpr float sourceMenuStep = 34;

float systemSectionTop(SystemStats const& system) {
    if (system.disks.empty()) return systemStatsBottom;
    const auto rows = (system.disks.size() + storageColumns - 1) / storageColumns;
    return storageCardsTop + static_cast<float>(rows) * storageCardPitch;
}

std::wstring freeSpaceLabel(uint64_t bytes) {
    constexpr uint64_t gb = 1'000'000'000;
    constexpr uint64_t tb = 1'000 * gb;
    if (bytes >= tb) {
        const auto tenths = (bytes / (tb / 100) + 5) / 10;
        return std::to_wstring(tenths / 10) + L"." + std::to_wstring(tenths % 10) + L" TB free";
    }
    if (bytes >= gb) return std::to_wstring(bytes / gb) + L" GB free";
    if (bytes > 0 && bytes < 1'000'000) return L"<1 MB free";
    return std::to_wstring(bytes / 1'000'000) + L" MB free";
}

float pickerHeight(size_t rows) { return 2 * pickerInset + pickerRowHeight * static_cast<float>(rows); }

float sourcePickerHeight(size_t count) {
    return 2 * pickerInset + sourceMenuStep * static_cast<float>((count + sourceColumns - 1) / sourceColumns);
}
constexpr float maxLyricsWidth = 338;
constexpr float panelRadius = 22;
constexpr float padTop = 13;
constexpr float padX = 14;
constexpr float mediaTitleLeft = padX + mediaCoverSize + 12;
constexpr float mediaTitleWidth = mediaPanelWidth - mediaTitleLeft - padX;
constexpr float padBottom = 8;
constexpr float innerWidth = panelWidth - 2 * padX;
constexpr float navHeight = 31;
constexpr float compactNavHeight = 20;
constexpr auto navRevealTime = 150ms;
constexpr float keepOpenMargin = 28;
constexpr float pi = 3.14159265f;

// Liquid split, from preview V7. The bubble slides out of the pill's right cap on a metaball neck,
// the neck pinches at pinchAt, then each side's tip retracts into its own cap.
constexpr float capRadius = pillHeight / 2;
constexpr float pinchAt = 0.55f;
constexpr float releaseGap = 10;      // gap between pill and bubble at the pinch and at rest
constexpr float neckSpread = 0.5f;
constexpr float handleSize = 2.4f;
constexpr float maxBubble = 259;

constexpr auto openDelay = 50ms;
constexpr auto closeDelay = 185ms;
constexpr auto featurePreviewTime = 850ms;
constexpr auto shapeTime = 500ms;
constexpr auto screenFadeTime = 310ms;
constexpr auto captureTime = 230ms;
constexpr float captureSlot = 19;
constexpr float privacyHeight = 30;
constexpr float privacyPadding = 6;
constexpr float privacyGap = 8;
constexpr auto themeTime = 700ms;
constexpr auto splitTime = 1152ms;
constexpr float splitRate = 5.0f / 6.0f;
constexpr auto bubbleHoldTime = 1200ms;
constexpr auto shortBubbleHoldTime = 450ms;

bool isCountdownAlert(IslandAlert::Kind kind) {
    return kind == IslandAlert::Kind::timerDone || kind == IslandAlert::Kind::focusDone ||
           kind == IslandAlert::Kind::breakDone;
}

auto alertHoldTime(IslandAlert const& alert) {
    if (alert.kind == IslandAlert::Kind::mediaStarted) return std::min(alert.hold, 650ms);
    const bool brief = alert.kind == IslandAlert::Kind::camera || isCountdownAlert(alert.kind);
    return std::min(alert.hold, brief ? shortBubbleHoldTime : bubbleHoldTime);
}
constexpr auto visibilityTime = 200ms;
constexpr auto lyricScrollTime = 190ms;
constexpr auto pillLyricTime = 250ms;
constexpr auto pillLyricSwitchTime = 750ms;

bool isCaptureAlert(IslandAlert::Kind kind) {
    return kind == IslandAlert::Kind::camera || kind == IslandAlert::Kind::microphone ||
           kind == IslandAlert::Kind::screenCapture;
}
constexpr int timerPresets[]{1, 5, 10};
constexpr int timerMaxSeconds = 120 * 60;
constexpr int breakMaxSeconds = 30 * 60;
constexpr int focusRounds = 4;        // the last break of a round is a long one
constexpr int longBreakFactor = 3;
constexpr size_t visibleDropRows = 4;
constexpr float dropRowHeight = 43;
constexpr float dropEmptyHeight = 222;
constexpr float dropFilledBaseHeight = 154;

constexpr unsigned inkColor = 0xf5f6f8;
constexpr unsigned mutedColor = 0xa4aab4;
constexpr unsigned readoutColor = 0xc2e0ff;
constexpr float batteryCellWidth = 25;   // 20 DIP cell, its nub, and the gap before the percentage

unsigned batteryColor(unsigned level) {
    return level <= 10 ? 0xff453a : level <= 20 ? 0xff9f0a : 0x30d158;
}

D2D1_COLOR_F rgb(unsigned value, float alpha = 1) {
    return D2D1::ColorF(value, alpha);
}

D2D1_COLOR_F white(float alpha) {
    return D2D1::ColorF(1, 1, 1, alpha);
}

D2D1_COLOR_F withAlpha(D2D1_COLOR_F color, float alpha) {
    color.a = alpha;
    return color;
}

D2D1_COLOR_F mixColor(D2D1_COLOR_F from, D2D1_COLOR_F to, float t) {
    return D2D1::ColorF(std::lerp(from.r, to.r, t), std::lerp(from.g, to.g, t), std::lerp(from.b, to.b, t),
                        std::lerp(from.a, to.a, t));
}

D2D1_RECT_F box(float x, float y, float width, float height) {
    return D2D1::RectF(x, y, x + width, y + height);
}

D2D1_RECT_F grow(D2D1_RECT_F rect, float x, float y) {
    return D2D1::RectF(rect.left - x, rect.top - y, rect.right + x, rect.bottom + y);
}

D2D1_RECT_F unite(D2D1_RECT_F a, D2D1_RECT_F b) {
    return D2D1::RectF(std::min(a.left, b.left), std::min(a.top, b.top), std::max(a.right, b.right),
                       std::max(a.bottom, b.bottom));
}

D2D1_RECT_F mixRect(D2D1_RECT_F a, D2D1_RECT_F b, float t) {
    return D2D1::RectF(std::lerp(a.left, b.left, t), std::lerp(a.top, b.top, t), std::lerp(a.right, b.right, t),
                       std::lerp(a.bottom, b.bottom, t));
}

bool inside(D2D1_RECT_F rect, D2D1_POINT_2F point) {
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}

D2D1_ROUNDED_RECT rounded(D2D1_RECT_F rect, float radius) {
    return D2D1::RoundedRect(rect, radius, radius);
}

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float lin(float from, float to, float value) {
    return clamp01((value - from) / (to - from));
}

float easeInOut(float t) {
    return t < 0.5f ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3.0f) / 2;
}

float easeOut(float t) {
    return 1 - std::pow(1 - t, 3.0f);
}

// CSS cubic-bezier(x1, y1, x2, y2) evaluated at x.
float bezier(float x1, float y1, float x2, float y2, float x) {
    const auto sample = [](float a, float b, float t) {
        return 3 * a * (1 - t) * (1 - t) * t + 3 * b * (1 - t) * t * t + t * t * t;
    };
    float low = 0, high = 1, t = x;
    for (int step = 0; step < 24; ++step) {
        if (sample(x1, x2, t) < x) low = t; else high = t;
        t = (low + high) / 2;
    }
    return sample(y1, y2, t);
}

float progress(Island::Clock::time_point start, Island::Clock::duration length, Island::Clock::time_point now) {
    if (length.count() <= 0 || now - start >= length) return 1;
    return clamp01(std::chrono::duration<float>(now - start).count() / std::chrono::duration<float>(length).count());
}

float elapsedMs(Island::Clock::time_point start, Island::Clock::time_point now) {
    return std::chrono::duration<float, std::milli>(now - start).count();
}

using Point = D2D1_POINT_2F;

Point pt(float x, float y) {
    return D2D1::Point2F(x, y);
}

Point add(Point a, Point v, float scale) {
    return pt(a.x + v.x * scale, a.y + v.y * scale);
}

Point half(Point a, Point b) {
    return pt((a.x + b.x) / 2, (a.y + b.y) / 2);
}

Point flip(Point a) {
    return pt(a.x, 2 * pillCenterY - a.y);
}

float distance(Point a, Point b) {
    return std::hypot(b.x - a.x, b.y - a.y);
}

float arcHandle(float angle, float radius) {
    return 4.0f / 3 * std::tan(angle / 4) * radius;
}

// SVG-style path building onto a Direct2D path geometry.
class PathBuilder {
public:
    explicit PathBuilder(ID2D1Factory* factory) {
        winrt::check_hresult(factory->CreatePathGeometry(path_.put()));
        winrt::check_hresult(path_->Open(sink_.put()));
        sink_->SetFillMode(D2D1_FILL_MODE_WINDING);
    }
    PathBuilder& move(Point to, bool filled = true) {
        end();
        sink_->BeginFigure(to, filled ? D2D1_FIGURE_BEGIN_FILLED : D2D1_FIGURE_BEGIN_HOLLOW);
        open_ = true;
        return *this;
    }
    PathBuilder& line(Point to) {
        sink_->AddLine(to);
        return *this;
    }
    PathBuilder& arc(Point to, float radius, bool large, bool clockwise) {
        sink_->AddArc(D2D1::ArcSegment(to, D2D1::SizeF(radius, radius), 0,
            clockwise ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
            large ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
        return *this;
    }
    PathBuilder& curve(Point a, Point b, Point to) {
        sink_->AddBezier(D2D1::BezierSegment(a, b, to));
        return *this;
    }
    PathBuilder& close() {
        if (open_) sink_->EndFigure(D2D1_FIGURE_END_CLOSED);
        open_ = false;
        return *this;
    }
    winrt::com_ptr<ID2D1PathGeometry> finish() {
        end();
        winrt::check_hresult(sink_->Close());
        return path_;
    }

private:
    void end() {
        if (open_) sink_->EndFigure(D2D1_FIGURE_END_OPEN);
        open_ = false;
    }

    winrt::com_ptr<ID2D1PathGeometry> path_;
    winrt::com_ptr<ID2D1GeometrySink> sink_;
    bool open_ = false;
};

void capsule(PathBuilder& path, D2D1_RECT_F rect) {
    const float radius = std::min(rect.right - rect.left, rect.bottom - rect.top) / 2;
    path.move(pt(rect.left + radius, rect.top)).line(pt(rect.right - radius, rect.top))
        .arc(pt(rect.right - radius, rect.bottom), radius, false, true).line(pt(rect.left + radius, rect.bottom))
        .arc(pt(rect.left + radius, rect.top), radius, false, true).close();
}

// The pill's right cap (center bx, radius R) joined to a bubble (center bx + d, radius r) by a
// metaball neck that stays tangent to both circles. k pulls the neck's waist toward the axis.
struct Neck {
    float alpha, beta, la, lb, cx, lenIn, lenOut;
    Point A, B, dirA, dirB, q0, q2, waist, into, out;
};

Neck neck(float bx, float d, float r, float k) {
    constexpr float R = capRadius;
    constexpr float cy = pillCenterY;
    Neck n{};
    n.cx = bx + d;
    const bool overlap = d < R + r;
    const float u1 = overlap ? std::acos(std::clamp((R * R + d * d - r * r) / (2 * R * d), -1.0f, 1.0f)) : 0;
    const float u2 = overlap ? std::acos(std::clamp((r * r + d * d - R * R) / (2 * r * d), -1.0f, 1.0f)) : 0;
    const float maxSpread = std::acos(std::clamp((R - r) / d, -1.0f, 1.0f));
    n.alpha = u1 + (maxSpread - u1) * neckSpread;
    n.beta = u2 + (pi - u2 - maxSpread) * neckSpread;
    n.A = pt(bx + R * std::cos(n.alpha), cy - R * std::sin(n.alpha));
    n.B = pt(n.cx - r * std::cos(n.beta), cy - r * std::sin(n.beta));
    n.dirA = pt(std::sin(n.alpha), std::cos(n.alpha));
    n.dirB = pt(std::sin(n.beta), -std::cos(n.beta));
    const float reach = std::min(neckSpread * handleSize, distance(n.A, n.B) / (R + r)) *
                        std::min(1.0f, 2 * d / (R + r));
    n.la = R * reach;
    n.lb = r * reach;
    // Split the upper curve at its middle so the midpoint can be pinched.
    const Point p1 = add(n.A, n.dirA, n.la);
    const Point p2 = add(n.B, n.dirB, -n.lb);
    n.q0 = half(n.A, p1);
    const Point q1 = half(p1, p2);
    n.q2 = half(p2, n.B);
    const Point r0 = half(n.q0, q1);
    const Point r1 = half(q1, n.q2);
    const Point m = half(r0, r1);
    n.lenIn = distance(r0, m);
    n.lenOut = distance(m, r1);
    float span = distance(r0, r1);
    if (span == 0) span = 1;
    // Pinch: lower the waist toward the axis, but never below the union of the two circles.
    const auto depth = [](float x, float c, float radius) {
        return std::sqrt(std::max(0.0f, radius * radius - (x - c) * (x - c)));
    };
    const float floor = std::max(depth(m.x, bx, R), depth(m.x, n.cx, r));
    n.waist = pt(m.x, cy - std::max(floor, (cy - m.y) * k));
    float dx = std::lerp(1.0f, (r1.x - r0.x) / span, k);
    float dy = (r1.y - r0.y) / span * k;
    float length = std::hypot(dx, dy);
    if (length == 0) length = 1;
    dx /= length;
    dy /= length;
    n.into = pt(n.waist.x - dx * n.lenIn, n.waist.y - dy * n.lenIn);
    n.out = pt(n.waist.x + dx * n.lenOut, n.waist.y + dy * n.lenOut);
    return n;
}

void connectedPath(PathBuilder& path, float left, float bx, float d, float r, float k) {
    constexpr float R = capRadius;
    constexpr float top = pillTop;
    constexpr float bottom = pillTop + pillHeight;
    const auto n = neck(bx, d, r, k);
    const float start = left + R;
    path.move(pt(start, top)).line(pt(bx, top)).arc(n.A, R, false, true)
        .curve(n.q0, n.into, n.waist).curve(n.out, n.q2, n.B)
        .arc(flip(n.B), r, n.beta < pi / 2, true)
        .curve(flip(n.q2), flip(n.out), flip(n.waist)).curve(flip(n.into), flip(n.q0), flip(n.A))
        .arc(pt(bx, bottom), R, false, true).line(pt(start, bottom)).arc(pt(start, top), R, false, true).close();
}

// The neck at the exact pinch moment, relative to bx = 0. The release phase starts from it.
Neck const& pinchShape() {
    static const Neck shape = neck(0, 2 * capRadius + releaseGap, capRadius, 0);
    return shape;
}

// After the pinch: each side keeps a tip that slides back and rounds into its cap (q: 0 -> 1).
void releasedPath(PathBuilder& path, float left, float bx, float q, float bubbleLeft, float bubbleWidth, bool includePill = true) {
    constexpr float R = capRadius;
    constexpr float cy = pillCenterY;
    constexpr float top = pillTop;
    constexpr float bottom = pillTop + pillHeight;
    const auto& s = pinchShape();
    const float phi = q * pi / 2;   // tip tangent: 0 = needle, 90 degrees = round
    const Point u = pt(std::cos(phi), std::sin(phi));
    const float start = left + R;
    const float arcA = arcHandle(s.alpha, R);
    const Point A = pt(bx + s.A.x, s.A.y);
    const Point hA = add(A, s.dirA, std::lerp(s.la / 2, arcA, q));
    const Point T = pt(std::lerp(bx + s.waist.x, bx + R, q), cy);
    const Point hT = add(T, u, -std::lerp(s.lenIn, arcA, q));
    if (includePill) {
        path.move(pt(start, top)).line(pt(bx, top)).arc(A, R, false, true)
            .curve(hA, hT, T).curve(flip(hT), flip(hA), flip(A))
            .arc(pt(bx, bottom), R, false, true).line(pt(start, bottom)).arc(pt(start, top), R, false, true).close();
    }
    const float cc = bubbleLeft + R;
    const float right = bubbleLeft + bubbleWidth - R;
    const float shift = cc - s.cx;
    const float arcB = arcHandle(s.beta, R);
    const Point B = pt(s.B.x + shift, s.B.y);
    const Point hB = add(B, s.dirB, -std::lerp(s.lb / 2, arcB, q));
    const Point T2 = pt(std::lerp(s.waist.x + shift, cc - R, q), cy);
    const Point hT2 = add(T2, pt(u.x, -u.y), std::lerp(s.lenOut, arcB, q));
    path.move(B).arc(pt(cc, top), R, false, true).line(pt(right, top)).arc(pt(right, bottom), R, false, true)
        .line(pt(cc, bottom)).arc(flip(B), R, false, true)
        .curve(flip(hB), flip(hT2), T2).curve(hT2, hB, B).close();
}

struct LiquidSplit {
    float left;
    D2D1_RECT_F bubble;
};

LiquidSplit makeLiquidSplit(PathBuilder& path, float width, float target, float p, bool anchored) {
    constexpr float neckTravel = 2 * capRadius + releaseGap;
    const float extraWidth = std::max(0.0f, target - pillHeight);
    const float travel = lin(0, pinchAt, p);
    const float widen = lin(pinchAt, 1, p);
    const float pinchSpeed = 0.8f * std::min(neckTravel / pinchAt, extraWidth / (1 - pinchAt));
    const auto smooth = [](float t) { return t * t * (3 - 2 * t); };
    const float distance = neckTravel * smooth(travel) + pinchSpeed * pinchAt * travel * travel * (travel - 1);
    const float tail = 1 - widen;
    const float bubbleWidth = pillHeight + extraWidth * smooth(widen) +
                              pinchSpeed * (1 - pinchAt) * widen * tail * tail;
    const float span = p < pinchAt ? width + distance : width + releaseGap + bubbleWidth;
    const float left = anchored ? 0 : -span / 2;
    const float cap = left + width - capRadius;
    if (p < pinchAt) {
        const float neck = 1 - smooth(lin(0.23f, pinchAt, p));
        if (distance < 0.5f) capsule(path, box(left, pillTop, width, pillHeight));
        else connectedPath(path, left, cap, distance, capRadius, neck);
        return {left, box(cap + distance - capRadius, pillTop, pillHeight, pillHeight)};
    }
    const float tip = smooth(lin(pinchAt, pinchAt + 0.28f, p));
    const auto bubble = box(left + width + releaseGap, pillTop, bubbleWidth, pillHeight);
    releasedPath(path, left, cap, tip, bubble.left, bubbleWidth);
    return {left, bubble};
}

std::wstring timestamp(double seconds, bool showHours = false) {
    const auto total = static_cast<long long>(std::max(0.0, seconds));
    const auto padded = [](long long value) { return (value < 10 ? L"0" : L"") + std::to_wstring(value); };
    if (showHours) return std::to_wstring(total / 3600) + L":" + padded(total / 60 % 60) + L":" + padded(total % 60);
    return std::to_wstring(total / 60) + L":" + padded(total % 60);
}

std::wstring countdownTime(int seconds) {
    auto value = timestamp(seconds);
    if (seconds < 600) value.insert(value.begin(), L'0');
    return value;
}

D2D1_RECT_F countdownShake(D2D1_RECT_F shape, float phase) {
    if (phase >= 1) return shape;
    const float offset = 4 * std::sin(2 * pi * phase) * (1 - phase);
    const float squeeze = 2 * std::sin(pi * phase);
    shape.left += offset + squeeze;
    shape.right += offset - squeeze;
    return shape;
}

// "SpotifyAB.SpotifyMusic_zpdnekdrzrea0!Spotify" -> "Spotify", "Spotify.exe" -> "Spotify".
std::wstring appName(std::wstring_view id) {
    std::wstring name(id);
    if (const auto bang = name.rfind(L'!'); bang != std::wstring::npos) name.erase(0, bang + 1);
    if (const auto slash = name.find_last_of(L"\\/"); slash != std::wstring::npos) name.erase(0, slash + 1);
    if (name.size() > 4 && _wcsicmp(name.c_str() + name.size() - 4, L".exe") == 0) name.resize(name.size() - 4);
    return name;
}

std::wstring processPath(std::wstring const& exe) {
    const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return {};
    std::wstring path;
    PROCESSENTRY32W entry{sizeof(PROCESSENTRY32W)};
    for (BOOL more = Process32FirstW(snapshot, &entry); more && path.empty(); more = Process32NextW(snapshot, &entry)) {
        if (_wcsicmp(entry.szExeFile, exe.c_str()) != 0) continue;
        if (const auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID)) {
            wchar_t buffer[MAX_PATH * 2];
            DWORD size = static_cast<DWORD>(std::size(buffer));
            if (QueryFullProcessImageNameW(process, 0, buffer, &size)) path.assign(buffer, size);
            CloseHandle(process);
        }
    }
    CloseHandle(snapshot);
    return path;
}

// The app's logo: from its Start menu entry, or else from its running executable.
bool loadIcon(std::wstring const& appId, int side, std::vector<uint8_t>& pixels, unsigned& width, unsigned& height) {
    winrt::com_ptr<IShellItemImageFactory> factory;
    const auto open = [&](std::wstring const& path) {
        factory = nullptr;
        return !path.empty() && SUCCEEDED(SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(factory.put())));
    };
    // A full executable path (app volume rows) opens directly; media sources use AppUserModelIDs.
    const bool path = appId.size() > 2 && appId[1] == L':';
    if (path ? !open(appId) : !open(L"shell:AppsFolder\\" + appId)) {
        auto exe = appName(appId) + L".exe";
        if (!open(processPath(exe))) return false;
    }
    HBITMAP handle = nullptr;
    if (FAILED(factory->GetImage(SIZE{side, side}, SIIGBF_ICONONLY, &handle)) || !handle) return false;
    BITMAP info{};
    GetObjectW(handle, sizeof(info), &info);
    width = static_cast<unsigned>(info.bmWidth);
    height = static_cast<unsigned>(std::abs(info.bmHeight));
    BITMAPINFO header{};
    header.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    header.bmiHeader.biWidth = info.bmWidth;
    header.bmiHeader.biHeight = -static_cast<LONG>(height);
    header.bmiHeader.biPlanes = 1;
    header.bmiHeader.biBitCount = 32;
    header.bmiHeader.biCompression = BI_RGB;
    pixels.assign(static_cast<size_t>(width) * height * 4, 0);
    const auto screen = GetDC(nullptr);
    const int rows = GetDIBits(screen, handle, 0, height, pixels.data(), &header, DIB_RGB_COLORS);
    ReleaseDC(nullptr, screen);
    DeleteObject(handle);
    if (rows != static_cast<int>(height) || width == 0) return false;
    // Shell images are usually premultiplied. Fix the ones that are not, or that carry no alpha at all.
    bool anyAlpha = false, straight = false;
    for (size_t i = 0; i < pixels.size(); i += 4) {
        anyAlpha = anyAlpha || pixels[i + 3] != 0;
        straight = straight || std::max({pixels[i], pixels[i + 1], pixels[i + 2]}) > pixels[i + 3];
    }
    for (size_t i = 0; i < pixels.size(); i += 4) {
        if (!anyAlpha) {
            pixels[i + 3] = 255;
        } else if (straight) {
            for (int c = 0; c < 3; ++c) pixels[i + c] = static_cast<uint8_t>(pixels[i + c] * pixels[i + 3] / 255);
        }
    }
    return true;
}

}

Island::~Island() {
    if (originalBitmap_) SelectObject(surface_, originalBitmap_);
    if (bitmap_) DeleteObject(bitmap_);
    if (surface_) DeleteDC(surface_);
}

// ---- State ---------------------------------------------------------------------------------------

void Island::setAnimate(bool animate) {
    animate_ = animate;
}

void Island::setPreferences(Settings const& settings) {
    const bool newFocus = settings.focusPreset != preferences_.focusPreset;
    const bool newBreak = settings.breakPreset != preferences_.breakPreset;
    const bool hideClipboardPreview = preferences_.showClipboardPreviews && !settings.showClipboardPreviews;
    preferences_ = settings;
    if (newFocus) focusSeconds_ = settings.focusMinutes() * 60;
    if (newBreak) {
        breakSeconds_ = settings.breakMinutes() * 60;
        longBreakSeconds_.reset();
    }
    setAutoCycle(settings.autoCycle);
    if (newFocus || newBreak) selectFocusPhase(breakPhase_);
    screens_.clear();
    for (const auto feature : settings.features) {
        if (settings.enabled(feature)) screens_.push_back(feature);
    }
    if (settings.enabled(Screen::prayer)) screens_.push_back(Screen::prayer);
    const auto now = Clock::now();
    if (!settings.enabled(screen_) && !screens_.empty()) showScreen(screens_.front(), now);
    if (!settings.enabled(Screen::drop)) dropHover_ = false;
    const auto dismiss = [&](IslandAlert const& alert) {
        return !alertAllowed(alert.kind) || (hideClipboardPreview && alert.kind == IslandAlert::Kind::copy);
    };
    if (queued_ && dismiss(*queued_)) queued_.reset();
    if (alert_ && dismiss(*alert_)) finishAlert(now);
    updatePill(now);
}

void Island::setMedia(MediaState const& media) {
    if (media.appId != media_.appId || media.sourceId != media_.sourceId) spectrum_ = {};
    const auto now = Clock::now();
    const bool introduce = animate_ && !suppressed_ && media.hasSession &&
        media.playing && !media_.playing &&
        ((pillMode_ == PillMode::timer && quick_.running()) || (pillMode_ == PillMode::focus && focus_.running()));
    if (!media.hasSession || media.playing || media.appId != media_.appId ||
        media.sourceId != media_.sourceId || media.title != media_.title) {
        pausedMediaRevealAt_ = {};
        compactPausedMediaAfterTimer_ = false;
    }
    if (!media.hasSession || media.playing) {
        pausedAt_ = {};
    } else if (!media_.hasSession || media_.playing || media_.appId != media.appId ||
               media_.sourceId != media.sourceId || media_.title != media.title) {
        pausedAt_ = now;
    }
    if (pressed_ == Hit::source && (pressedIndex_ >= media.sources.size() || pressedIndex_ >= media_.sources.size() ||
        media.sources[pressedIndex_].appId != media_.sources[pressedIndex_].appId ||
        media.sources[pressedIndex_].id != media_.sources[pressedIndex_].id)) pressed_ = Hit::none;
    if (pressed_ == Hit::cover && (media_.appId != media.appId || media_.sourceId != media.sourceId)) pressed_ = Hit::none;
    if (!media.hasSession || media.sources.size() < 2) sourcePickerOpen_ = false;
    // The media screen changes height when a session comes or goes; animate that like a screen change.
    if (open_ && screen_ == Screen::media && media.hasSession != media_.hasSession) {
        heightFrom_ = panelHeight(now);
        screenFrom_ = screen_;
        screenStart_ = animate_ ? now : Clock::time_point{};
        widthFrom_ = panelWidthAt(now);
        widthStart_ = screenStart_;
    }
    if (media.cover != coverSource_) {
        coverSource_ = media.cover;
        cover_ = {};
        if (coverSource_) cover_ = {coverSource_->size, coverSource_->size, coverSource_->pixels, nullptr};
    }
    if (media.playing && retiringDotMode_ == PillMode::media) dotExitStart_ = {};
    if (media_.playing && !media.playing) retirePausedActivity(PillMode::media, now);
    media_ = media;
    if (!media_.playing) {
        mediaIntroPending_ = false;
        if (queued_ && queued_->kind == IslandAlert::Kind::mediaStarted) queued_.reset();
        if (alert_ && alert_->kind == IslandAlert::Kind::mediaStarted) finishAlert(now);
    }
    updatePill(now);
    if (introduce && activityCompanion() == PillMode::media) {
        mediaIntroPending_ = open_ || panelValue(now) > 0;
        if (!mediaIntroPending_) notify({IslandAlert::Kind::mediaStarted, title()});
    }
}

void Island::setPower(PowerState const& power) {
    power_ = power;
}

void Island::setPrayerSchedule(std::optional<PrayerSchedule> const& schedule) {
    prayerSchedule_ = schedule;
}

void Island::setNetwork(NetworkStatus const& status) {
    network_ = status;
}

void Island::setBluetoothDevices(std::vector<ConnectedBluetoothDevice> devices) {
    bluetoothOffset_ = std::min(bluetoothOffset_, devices.size() - std::min(devices.size(), visibleBluetoothRows));
    bluetoothDevices_ = std::move(devices);
}

void Island::setSystem(SystemStats const& stats) {
    system_ = stats;
}

void Island::setAudioOutput(std::wstring const& output) { audioOutput_ = output; }

void Island::setAudioOutputs(std::vector<AudioOutput> outputs) {
    audioOutputOffset_ = std::min(audioOutputOffset_, outputs.size() - std::min(outputs.size(), visibleAudioOutputs));
    if (pressed_ == Hit::audioOutputChoice && (pressedIndex_ >= outputs.size() || pressedIndex_ >= audioOutputs_.size() ||
        outputs[pressedIndex_].id != audioOutputs_[pressedIndex_].id)) pressed_ = Hit::none;
    audioOutputs_ = std::move(outputs);
}

void Island::setAppVolumes(std::vector<AppVolume> apps) {
    const size_t limit = apps.size() - std::min(apps.size(), visibleAppRows);
    appOffset_ = std::min(appOffset_, limit);
    if ((pressed_ == Hit::appMute) && (pressedIndex_ >= apps.size() || pressedIndex_ >= appVolumes_.size() ||
        apps[pressedIndex_].id != appVolumes_[pressedIndex_].id)) pressed_ = Hit::none;
    appVolumes_ = std::move(apps);
}

std::optional<IslandCommand> Island::takeLiveCommand() {
    return std::exchange(liveCommand_, std::nullopt);
}

bool Island::setWindowsNotificationsQuiet(std::optional<bool> quiet) {
    if (windowsNotificationsQuiet_ == quiet) return false;
    windowsNotificationsQuiet_ = quiet;
    if (alertsQuiet()) {
        queued_.reset();
        if (alert_) finishAlert(Clock::now());
    }
    return true;
}

void Island::setLyrics(LyricsView const& lyrics) { lyrics_ = &lyrics; }

void Island::setScratchpad(ScratchpadView const& scratchpad) { scratchpad_ = &scratchpad; }

void Island::setScratchEditing(bool editing) { scratchEditing_ = editing; }

void Island::setDropItems(std::vector<DropPreview> items) {
    const auto now = Clock::now();
    const float before = panelHeight(now);
    const float oldTarget = screenHeight(Screen::drop);
    const size_t limit = items.size() - std::min(items.size(), visibleDropRows);
    dropOffset_ = items.size() > dropItems_.size() ? limit : std::min(dropOffset_, limit);
    dropItems_ = std::move(items);
    dropBitmaps_.clear();
    if (pressed_ == Hit::dropImage || pressed_ == Hit::removeDrop) pressed_ = Hit::none;
    if (open_ && screen_ == Screen::drop && screenHeight(screen_) != oldTarget) {
        heightFrom_ = before;
        keepHeight_ = std::max(keepHeight_, before);
        screenFrom_ = screen_;
        screenStart_ = animate_ ? now : Clock::time_point{};
    }
}

void Island::setPopupOpen(bool open) {
    popupOpen_ = open;
    closeAt_.reset();
}

void Island::setDropHover(bool hovering) {
    dropHover_ = hovering;
    if (hovering) {
        closeAt_.reset();
        showDropZone();
    }
}

void Island::showDropZone(bool hold) {
    if (suppressed_ || !preferences_.enabled(Screen::drop)) return;
    const auto now = Clock::now();
    if (bubbleShowing()) clearAlerts();
    setOpen(true, now);
    showScreen(Screen::drop, now);
    if (hold) {
        closeAt_ = now + 1200ms;
        buttonWasDown_ = true;
    }
}

std::optional<size_t> Island::dropItemAt(POINT client) const {
    const auto item = targetAt(stagePoint(client));
    return item && item->hit == Hit::dropItem ? std::optional<size_t>(item->index) : std::nullopt;
}

void Island::closePanel() { setOpen(false, Clock::now()); }

bool Island::panelPinned() const { return open_ && panelPinned_; }

std::optional<RECT> Island::scratchEditorRect() const {
    if (!open_ || screen_ != Screen::scratch || !scratchEditor_) return std::nullopt;
    const auto box = *scratchEditor_;
    return RECT{anchor_.x + std::lround(box.left * scale_), anchor_.y + std::lround(box.top * scale_),
                anchor_.x + std::lround(box.right * scale_), anchor_.y + std::lround(box.bottom * scale_)};
}

bool Island::wantsLyrics() const {
    return preferences_.enabled(Screen::media) && open_ && screen_ == Screen::media && media_.hasSession && lyricsOpen_;
}

bool Island::wantsLyricPill() const {
    return preferences_.showPillLyrics && !suppressed_ && lyricsOpen_ && pillMode_ == PillMode::media && media_.playing;
}

Island::MotionSample Island::sampleSpring(Clock::time_point start, Clock::duration duration, Clock::time_point now,
                                          float from, float to, float velocity, Spring spring) {
    if (now - start >= duration) return {to, 0};
    const float seconds = std::max(0.0f, std::chrono::duration<float>(now - start).count());
    // Scale the split spring's rate while preserving its damping ratio and velocity on reversal.
    const float damping = spring == Spring::split ? 18.0f * splitRate : 25.0f;
    const float stiffness = spring == Spring::split ? 78.0f * splitRate * splitRate : 190.0f;
    const float decayRate = damping / 2;
    const float frequency = std::sqrt(std::abs(stiffness - decayRate * decayRate));
    const float phase = frequency * seconds;
    const float displacement = from - to;
    const float slope = (velocity + decayRate * displacement) / frequency;
    const float cosine = spring == Spring::split ? std::cosh(phase) : std::cos(phase);
    const float sine = spring == Spring::split ? std::sinh(phase) : std::sin(phase);
    const float curve = displacement * cosine + slope * sine;
    const float curveSpeed = frequency * ((spring == Spring::split ? displacement : -displacement) * sine + slope * cosine);
    const float decay = std::exp(-decayRate * seconds);
    const float value = to + decay * curve;
    if (value < 0 || value > 1) return {clamp01(value), 0};
    return {value, decay * (curveSpeed - decayRate * curve)};
}

Island::MotionSample Island::pillMotion(Clock::time_point now) const {
    if (!animate_) return {pillTarget_, 0};
    return sampleSpring(pillStart_, shapeTime, now, pillFrom_, pillTarget_, pillVelocity_);
}

float Island::pillAmount(Clock::time_point now) const {
    return pillMotion(now).value;
}

float Island::pillTransitionProgress(Clock::time_point now) const {
    return animate_ ? progress(pillTransitionStart_, pillTransitionTime_, now) : 1;
}

bool Island::pillLyricGrowing() const {
    return pillSizeTarget_.width > pillSizeFrom_.width || pillSizeTarget_.height > pillSizeFrom_.height;
}

float Island::pillLyricOut(float t) const {
    if (pillTransitionTime_ == pillLyricSwitchTime) return easeInOut(lin(0, 0.38f, t));
    return easeInOut(lin(0, pillLyricGrowing() ? 0.72f : 0.55f, t));
}

float Island::pillLyricIn(float t) const {
    if (pillTransitionTime_ == pillLyricSwitchTime) return easeInOut(lin(0.40f, 0.86f, t));
    return easeOut(lin(pillLyricGrowing() ? 0.52f : 0.32f, 1, t));
}

float Island::pillTitleOpacity(Clock::time_point now) const {
    const float t = pillTransitionProgress(now);
    const float phase = pillTitleVisible_ ? lin(0.38f, 0.85f, t) : lin(0, 0.42f, t);
    return std::lerp(pillTitleFrom_, pillTitleVisible_ ? 1.0f : 0.0f, easeInOut(phase));
}

D2D1_SIZE_F Island::pillSize(Clock::time_point now) const {
    if (pillSizeTarget_.width == 0) return {pillWidth, pillHeight};
    const float t = pillTransitionProgress(now);
    const auto resize = [t](float from, float to) {
        const float phase = to >= from ? lin(0, 0.64f, t) : lin(0.46f, 1, t);
        return std::lerp(from, to, easeInOut(phase));
    };
    return {resize(pillSizeFrom_.width, pillSizeTarget_.width),
            resize(pillSizeFrom_.height, pillSizeTarget_.height)};
}

float Island::captureProgress(Clock::time_point now, CaptureIndicator const& indicator) const {
    const auto amount = animate_ ? easeInOut(progress(indicator.start, captureTime, now)) : 1.0f;
    return std::lerp(indicator.from, indicator.shown ? 1.0f : 0.0f, amount);
}

std::array<float, 3> Island::captureAmounts(Clock::time_point now) const {
    std::array<float, 3> amounts{};
    for (size_t i = 0; i < amounts.size(); ++i) amounts[i] = captureProgress(now, captureIndicators_[i]);
    return amounts;
}

D2D1_RECT_F Island::compactShape(Clock::time_point now, float amount) const {
    const float t = clamp01(amount);
    const auto size = pillSize(now);
    const float width = std::lerp(dotSize, size.width + privacyBadgeWidth(now), t);
    const float height = std::lerp(dotSize, size.height, t);
    auto shape = box(-width / 2, std::lerp(pillCenterY - dotSize / 2, pillTop, t), width, height);
    if (animate_ && countdownCompletion_.mode == pillMode_ && !open_ && t >= 1)
        shape = countdownShake(shape, progress(countdownCompletion_.start, countdownCompletionMotion, now));
    return shape;
}

float Island::privacyBadgeWidth(Clock::time_point now) const {
    if (pillContent_ == PillMode::privacy || pillMode_ == PillMode::dot) return 0;
    const auto [screen, camera, mic] = captureAmounts(now);
    return 14 * std::max({screen, camera, mic}) + privacyIconPitch * (screen + camera + mic);
}

D2D1_RECT_F Island::privacyBadgeRect(Clock::time_point now, D2D1_RECT_F pill) const {
    const float width = privacyBadgeWidth(now);
    return box(pill.right - 8 - width, (pill.top + pill.bottom - privacyBadgeHeight) / 2, width, privacyBadgeHeight);
}

Island::MotionSample Island::panelMotion(Clock::time_point now) const {
    const float target = open_ ? 1.0f : 0.0f;
    if (!animate_) return {target, 0};
    return sampleSpring(panelStart_, shapeTime, now, panelFrom_, target, panelVelocity_);
}

float Island::panelValue(Clock::time_point now) const {
    return panelMotion(now).value;
}

float Island::screenHeight(Screen screen) const {
    if (screens_.empty()) return mediaIdleHeight;
    switch (screen) {
    case Screen::media:
        if (sourcePickerOpen_) return std::max(mediaPanelHeight,
            navHeight + sourceMenuTop + sourcePickerHeight(std::min(media_.sources.size(), visibleSources)) + padBottom);
        return mediaPanelHeight;
    case Screen::timers: return 268;
    case Screen::scratch: return 216.0f;
    case Screen::drop: return dropItems_.empty() ? dropEmptyHeight :
        dropFilledBaseHeight + dropRowHeight * static_cast<float>(std::min(dropItems_.size(), visibleDropRows));
    case Screen::system: {
        const auto appRows = std::clamp<size_t>(appVolumes_.size(), 1, visibleAppRows);
        const auto devices = std::clamp<size_t>(bluetoothDevices_.size(), 1, visibleBluetoothRows);
        const float mixer = appRowsTop + static_cast<float>(appRows) * appRowHeight;
        const float connection = connectionTop + bluetoothRowsTop + static_cast<float>(devices) * bluetoothRowHeight + 29;
        const auto outputs = std::clamp<size_t>(audioOutputs_.size(), 1, visibleAudioOutputs);
        const float menu = audioPickerOpen_ ? systemAudioMenuTop + pickerHeight(outputs + 1) : 0;
        return navHeight + systemSectionTop(system_) + std::max({mixer, connection, menu}) + padBottom;
    }
    case Screen::prayer:
        return prayerPickerOpen_ ? std::max(prayerPanelHeight,
            navHeight + prayerMenuTop + pickerHeight(prayerLocations.size()) + padBottom) : prayerPanelHeight;
    }
    return 146.0f;
}

float Island::panelHeight(Clock::time_point now) const {
    const float height = screenHeight(screen_);
    return animate_ ? std::lerp(heightFrom_, height, sampleSpring(screenStart_, shapeTime, now, 0, 1).value) : height;
}

float Island::panelWidthAt(Clock::time_point now) const {
    const float width = screen_ == Screen::media
        ? mediaPanelWidth + (preferences_.enabled(Screen::media) && media_.hasSession && lyricsOpen_ ? lyricsWidth_ : 0)
        : screen_ == Screen::system ? systemPanelWidth
        : screen_ == Screen::prayer ? prayerPanelWidth : panelWidth;
    return animate_ ? std::lerp(widthFrom_, width, sampleSpring(widthStart_, shapeTime, now, 0, 1).value) : width;
}

D2D1_RECT_F Island::panelRect(Clock::time_point now) const {
    const float width = panelWidthAt(now);
    return box(-width / 2, pillTop, width, panelHeight(now) + inlineInset() + navHeightAt(now) - navHeight);
}

bool Island::moving(Clock::time_point now) const {
    if (!animate_) return false;
    return activitySwapActive(now) || now - timerDigitsStart_ < timerDigitMotion || now - countdownCompletion_.start < countdownCompletionMotion ||
           now - dotExitStart_ < activityDotExitTime || now - dotHoverStart_ < dotHoverTime || now - pillStart_ < shapeTime || now - panelStart_ < shapeTime || now - navStart_ < navRevealTime || now - navSelectionStart_ < navSelectionTime ||
           std::ranges::any_of(captureIndicators_, [now](auto const& item) { return now - item.start < captureTime; }) ||
           now - screenStart_ < shapeTime ||
           now - widthStart_ < shapeTime || (wantsLyrics() && now - lyricScrollStart_ < lyricScrollTime) ||
           now - themeStart_ < themeTime || now - pillTransitionStart_ < pillTransitionTime_ ||
           (bubbleShowing() && now - alertStart_ < splitTime);
}

std::wstring Island::title() const {
    if (!media_.title.empty()) return media_.title;
    const auto name = appName(media_.appId);
    return name.empty() ? L"Media playing" : name;
}

Island::PillMode Island::activityCompanion() const {
    const bool media = preferences_.enabled(Screen::media) && media_.hasSession && media_.playing;
    const bool timers = preferences_.enabled(Screen::timers);
    const bool focus = timers && focus_.running();
    const bool quick = timers && quick_.running();
    switch (pillMode_) {
    case PillMode::media:
        if (focus && quick) return quickView_ ? PillMode::timer : PillMode::focus;
        if (focus) return PillMode::focus;
        if (quick) return PillMode::timer;
        break;
    case PillMode::focus:
        if (media) return PillMode::media;
        if (quick) return PillMode::timer;
        break;
    case PillMode::timer:
        if (media) return PillMode::media;
        if (focus) return PillMode::focus;
        break;
    default: break;
    }
    return PillMode::dot;
}

bool Island::activitySwapActive(Clock::time_point now) const {
    const auto duration = activitySwap_.retireOutgoing ? activityHandoffTime : activitySwapTime;
    return animate_ && now - activitySwap_.start < duration &&
           pillMode_ == activitySwap_.incoming &&
           (activitySwap_.retireOutgoing || activityCompanion() == activitySwap_.outgoing);
}

void Island::retirePausedActivity(PillMode mode, Clock::time_point now) {
    if (suppressed_ || bubbleShowing()) return;
    const auto companion = activityCompanion();
    if (companion == PillMode::dot) return;
    if (pillMode_ != mode) {
        if (animate_ && !open_ && lastActivityDot_ && companion == mode) {
            activitySwap_.start = {};
            retiringDot_ = *lastActivityDot_;
            retiringDotMode_ = mode;
            dotExitStart_ = now;
        }
        return;
    }
    preferredPill_ = companion;
    if (!animate_) return;

    const auto pill = compactShape(now, pillAmount(now));
    const float top = (pill.top + pill.bottom - activityDotSize) / 2;
    const auto dot = lastActivityDot_.value_or(box(pill.right + activityDotGap, top,
                                                  activityWidth(companion, now), activityDotSize));
    const auto start = open_ ? now + shapeTime : now;
    activitySwap_ = {start, companion, mode, pill, dot, pillTitle_, true, false};
    if (open_ && !panelPinned_) setOpen(false, now);
    if (companion != PillMode::media) quickView_ = companion == PillMode::timer;
    openAt_.reset();
    previousPillLyric_ = {};
    pillLyric_ = {};
}

void Island::swapActivities(Clock::time_point now) {
    const auto incoming = activityCompanion();
    const bool primaryRunning = (pillMode_ == PillMode::media && media_.playing) ||
                                (pillMode_ == PillMode::focus && focus_.running()) ||
                                (pillMode_ == PillMode::timer && quick_.running());
    if (!primaryRunning || incoming == PillMode::dot || open_ || bubbleShowing()) return;
    const auto dot = std::ranges::find(targets_, Hit::activityDot, &Target::hit);
    if (dot == targets_.end() || dot->index != static_cast<size_t>(incoming)) return;

    auto label = pillLyric_.layout ? pillLyric_.text : pillTitle_;
    if (pillMode_ == PillMode::focus || pillMode_ == PillMode::timer) {
        const auto& countdown = pillMode_ == PillMode::timer ? quick_ : focus_;
        label += L"  " + timestamp(countdown.seconds(now));
    }
    const auto compact = compactShape(now, pillAmount(now));
    activitySwap_ = {animate_ ? now : Clock::time_point{}, incoming, pillMode_, compact,
                     grow(dot->rect, -2, -2), std::move(label)};
    if (incoming != PillMode::media) quickView_ = incoming == PillMode::timer;
    showScreen(incoming == PillMode::media ? Screen::media : Screen::timers, now);
    preferredPill_ = incoming;
    countdownCompletion_ = {};
    openAt_.reset();
    previewUntil_ = {};
    previousPillLyric_ = {};
    pillLyric_ = {};
    updatePill(now, false);
}

void Island::updatePill(Clock::time_point now, bool keepActivity) {
    auto mode = PillMode::dot;
    const bool timersEnabled = preferences_.enabled(Screen::timers);
    const bool focusActive = timersEnabled && (focus_.running() ||
        (alert_ && !alert_->fromCompanion && (alert_->kind == IslandAlert::Kind::focusDone || alert_->kind == IslandAlert::Kind::breakDone)));
    const bool timerActive = timersEnabled && (quick_.running() ||
        (alert_ && !alert_->fromCompanion && alert_->kind == IslandAlert::Kind::timerDone));
    const bool mediaEnabled = media_.hasSession && preferences_.enabled(Screen::media);
    const bool mediaActive = mediaEnabled && media_.playing;
    const bool currentActivity = (pillMode_ == PillMode::media && mediaActive) ||
                                 (pillMode_ == PillMode::focus && focusActive) ||
                                 (pillMode_ == PillMode::timer && timerActive);
    if (!open_ && now < previewUntil_) {
        mode = PillMode::preview;
    } else if (keepActivity && timersEnabled && now < countdownCompletion_.until) {
        mode = countdownCompletion_.mode;
    } else if (keepActivity && currentActivity && activityCompanion() != PillMode::dot) {
        mode = pillMode_;
    } else if (focusActive && preferredPill_ == PillMode::focus) {
        mode = PillMode::focus;
    } else if (timerActive && preferredPill_ == PillMode::timer) {
        mode = PillMode::timer;
    } else if (mediaActive) {
        mode = PillMode::media;
    } else if (focusActive) {
        mode = PillMode::focus;
    } else if (timerActive) {
        mode = PillMode::timer;
    } else if (timersEnabled && focus_.paused() && (preferredPill_ == PillMode::focus || !quick_.paused())) {
        mode = PillMode::focus;
    } else if (timersEnabled && quick_.paused()) {
        mode = PillMode::timer;
    } else if (mediaEnabled) {
        mode = now < pausedMediaRevealAt_ ? PillMode::dot : PillMode::media;
    } else if (std::ranges::any_of(captureIndicators_, &CaptureIndicator::shown)) {
        mode = PillMode::privacy;
    }
    pillMode_ = mode;
    if (mode != PillMode::dot) {
        pillContent_ = mode;
        if (mode == PillMode::media) pillTitle_ = title();
        else if (mode == PillMode::focus) {
            pillTitle_ = breakPhase_ ? L"Break" : L"Focus";
            if (autoCycle_) pillTitle_ += L" " + std::to_wstring(focusRound_) + L"/" + std::to_wstring(focusRounds);
        }
        else if (mode == PillMode::timer) pillTitle_ = L"Timer";
        else if (mode == PillMode::privacy) pillTitle_.clear();
        else pillTitle_ = screenLabel(screen_);
        pillPlaying_ = media_.playing;
    }
    const bool compact = pillOccluded_ && mode != PillMode::preview;
    if (bubbleShowing() && !compact) return;
    const float target = mode == PillMode::dot || compact ? 0.0f : 1.0f;
    if (target != pillTarget_) {
        const auto motion = pillMotion(now);
        pillFrom_ = motion.value;
        pillVelocity_ = motion.velocity;
        pillTarget_ = target;
        pillStart_ = now;
    }
}

void Island::updatePillLayout(Clock::time_point now, float workWidth) {
    if (bubbleShowing() || (pillMode_ == PillMode::dot && pillAmount(now) > 0)) return;
    const float t = pillTransitionProgress(now);
    if (t >= 1) previousPillLyric_ = {};

    std::wstring_view line;
    if (wantsLyricPill() && lyrics_ && lyrics_->status == LyricsStatus::ready && media_.duration > 0) {
        const auto& lines = lyrics_->lines;
        const auto next = std::ranges::upper_bound(lines, media_.positionAt(now) - lyrics_->delaySeconds(), {}, &LyricLine::seconds);
        if (next != lines.begin()) line = (next - 1)->text;
        if (line.find_first_not_of(L" \t\r\n") == std::wstring_view::npos || line.size() > 4096) line = {};
    }
    const auto captureCount = std::ranges::count_if(captureIndicators_, &CaptureIndicator::shown);
    const float captureSlots = captureSlot * static_cast<float>(captureCount);
    const float reserved = privacyGap + 2 * privacyPadding + captureSlot * static_cast<float>(captureIndicators_.size());
    const auto compactAt = pausedAt_ + std::chrono::seconds(preferences_.pauseSeconds());
    const bool showTitle = pillMode_ != PillMode::media || media_.playing ||
        (!compactPausedMediaAfterTimer_ && (preferences_.compactPause == 3 || now < compactAt));
    D2D1_SIZE_F target{pillWidth, pillHeight};
    if (pillMode_ == PillMode::privacy) {
        target = D2D1::SizeF(2 * privacyPadding + captureSlots, privacyHeight);
    } else if (pillMode_ == PillMode::focus || pillMode_ == PillMode::timer) {
        const auto& timer = pillMode_ == PillMode::timer ? quick_ : focus_;
        const bool completed = countdownCompletion_.mode == pillMode_ && now < countdownCompletion_.until;
        const auto& label = completed ? countdownCompletion_.label : pillTitle_;
        if (measuredTimerLabel_ != label) {
            measuredTimerLabel_ = label;
            timerLabelWidth_ = std::ceil(measure(label, pillTitleFont_.get()));
        }
        const float clock = completed ? 0 : 9 + timerDigitWidth_ * static_cast<float>(countdownTime(timer.seconds(now)).size());
        target.width = 46 + timerLabelWidth_ + clock;
    } else if (pillMode_ == PillMode::media) {
        const float trackWidth = std::max(pillWidth, workWidth - maxBubble - 60 - reserved);
        const float minimum = preferences_.fixedPillMinimum ? pillWidth : 114;
        if (pillTrackTitle_.text != pillTitle_ || pillTrackMaxWidth_ != trackWidth || pillTrackMinimum_ != minimum) {
            pillTrackTitle_.text = pillTitle_;
            pillTrackMaxWidth_ = trackWidth;
            pillTrackMinimum_ = minimum;
            auto& layout = pillTrackTitle_.layout;
            layout = nullptr;
            winrt::check_hresult(fonts_->CreateTextLayout(pillTitle_.data(), static_cast<UINT32>(pillTitle_.size()),
                pillTitleFont_.get(), trackWidth - pillTrackLeft - pillTrackRight, 10'000, layout.put()));
            winrt::check_hresult(layout->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP));
            const DWRITE_TRIMMING noTrimming{DWRITE_TRIMMING_GRANULARITY_NONE, 0, 0};
            winrt::check_hresult(layout->SetTrimming(&noTrimming, nullptr));
            winrt::check_hresult(layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
            DWRITE_TEXT_METRICS metrics{};
            winrt::check_hresult(layout->GetMetrics(&metrics));
            pillTrackTitle_.textSize = {metrics.width, metrics.height};
            pillTrackSize_ = {std::max(minimum, std::ceil(metrics.width) + pillTrackLeft + pillTrackRight),
                              std::max(pillHeight, std::ceil(metrics.height) + 16)};
        }
        target = showTitle ? pillTrackSize_ : D2D1::SizeF(pausedPillWidth, pausedPillHeight);
    }
    const float maxWidth = std::clamp(workWidth - maxBubble - 60 - reserved, 180.0f, maxPillLyricWidth);
    const float maxHeight = std::min(maxPillLyricHeight, maxWidth / 2);
    const bool sameLyric = line == pillLyric_.text && (line.empty() || maxWidth == pillLyricMaxWidth_);
    if (sameLyric && (pillLyric_.layout || (showTitle == pillTitleVisible_ &&
        target.width == pillSizeTarget_.width && target.height == pillSizeTarget_.height))) return;

    PillText next;
    if (!line.empty()) {
        next.text = line;
        winrt::check_hresult(fonts_->CreateTextLayout(line.data(), static_cast<UINT32>(line.size()),
            pillLyricFont_.get(), maxWidth - pillHeight - pillLyricIconSpace, 10'000, next.layout.put()));
        DWRITE_TEXT_METRICS metrics{};
        winrt::check_hresult(next.layout->GetMetrics(&metrics));
        if (metrics.lineCount > 1) {
            // Reserve space for both rounded ends even when a lyric needs several rows.
            winrt::check_hresult(next.layout->SetMaxWidth(maxWidth - maxHeight - pillLyricIconSpace));
            winrt::check_hresult(next.layout->GetMetrics(&metrics));
        }
        if (metrics.height + 16 <= maxHeight) {
            next.textSize = {metrics.layoutWidth, metrics.height};
            target = {metrics.lineCount > 1 ? maxWidth :
                        std::max(pillWidth, std::ceil(metrics.width) + pillHeight + pillLyricIconSpace),
                      std::max(pillHeight, std::ceil(metrics.height) + 16)};
        } else {
            next.layout = nullptr;
        }
    }
    const bool switchingLyric = static_cast<bool>(pillLyric_.layout) != static_cast<bool>(next.layout);
    const bool switchingPause = showTitle != pillTitleVisible_;
    const float outgoing = pillLyricOut(t);
    const float incoming = pillLyricIn(t);
    const float oldAlpha = previousPillAlpha_ * (1 - outgoing);
    if (oldAlpha > incoming) {
        previousPillAlpha_ = oldAlpha;
        previousPillOffset_ = std::lerp(previousPillOffset_, -pillLyricTravel, outgoing);
    } else {
        previousPillLyric_ = std::move(pillLyric_);
        previousPillAlpha_ = incoming;
        previousPillOffset_ = pillLyricTravel * (1 - incoming);
    }
    pillLyricMaxWidth_ = maxWidth;
    pillSizeFrom_ = pillSize(now);
    if ((pillMode_ == PillMode::privacy || (pillMode_ == PillMode::media && compactPausedMediaAfterTimer_)) &&
        pillAmount(now) <= 0) pillSizeFrom_ = target;
    pillSizeTarget_ = target;
    pillLyric_ = std::move(next);
    pillTitleFrom_ = pillTitleOpacity(now);
    pillTitleVisible_ = showTitle;
    pillTransitionTime_ = switchingLyric ? pillLyricSwitchTime : switchingPause ? shapeTime : pillLyricTime;
    pillTransitionStart_ = pillMode_ == PillMode::dot ? Clock::time_point{} : now;
    if (pillMode_ == PillMode::dot) previousPillLyric_ = {};
}

void Island::updateTheme(Clock::time_point now) {
    auto target = media_.hasSession && preferences_.enabled(Screen::media) && preferences_.accentMode == 0
        ? media_.theme : CoverTheme{};
    if (preferences_.accentMode == 2) target.accent = preferences_.customAccent;
    const auto fade = [&] {
        const float t = progress(themeStart_, themeTime, now);
        return t * t * (3 - 2 * t);
    };
    if (target != theme_) {
        // Start from whatever is on screen, so a track change mid-fade doesn't jump.
        const float shown = fade();
        accentFrom_ = animate_ ? mixColor(accentFrom_, rgb(theme_.accent), shown) : rgb(target.accent);
        tintFrom_ = animate_ ? mixColor(tintFrom_, rgb(theme_.tint), shown) : rgb(target.tint);
        theme_ = target;
        themeStart_ = animate_ ? now : Clock::time_point{};
    }
    const float t = fade();
    accent_ = mixColor(accentFrom_, rgb(theme_.accent), t);
    tint_ = mixColor(tintFrom_, rgb(theme_.tint), t);
}

float Island::visibility(Clock::time_point now) const {
    const float destination = suppressed_ ? 0.0f : 1.0f;
    if (!animate_) return destination;
    return std::lerp(visibilityFrom_, destination, easeOut(progress(visibilityStart_, visibilityTime, now)));
}

bool Island::visible() const {
    return visibility(Clock::now()) > 0;
}

void Island::setSuppressed(bool suppressed) {
    if (suppressed_ == suppressed) return;
    const auto now = Clock::now();
    visibilityFrom_ = visibility(now);
    visibilityStart_ = now;
    suppressed_ = suppressed;
    if (!suppressed_) return;
    previewUntil_ = {};
    clearAlerts();
    setOpen(false, now);
    setDotHover(false, Clock::now());
    hovered_ = Hit::none;
    pointerCancel();
}

bool Island::setPillOccluded(bool occluded) {
    if (pillOccluded_ == occluded) return false;
    pillOccluded_ = occluded;
    if (occluded) openAt_.reset();
    updatePill(Clock::now());
    return true;
}

std::optional<RECT> Island::pillFootprint() const {
    if (pillMode_ == PillMode::dot || pillMode_ == PillMode::preview || pillSizeTarget_.width <= 0)
        return std::nullopt;
    const auto now = Clock::now();
    const float halfWidth = (pillSizeTarget_.width + privacyBadgeWidth(now)) / 2;
    const auto companion = activityCompanion();
    const float extra = companion == PillMode::dot ? 0 : activityDotGap + activityWidth(companion, now);
    return RECT{anchor_.x - std::lround(halfWidth * scale_), anchor_.y + std::lround(pillTop * scale_),
                anchor_.x + std::lround((halfWidth + extra) * scale_),
                anchor_.y + std::lround((pillTop + pillSizeTarget_.height) * scale_)};
}

bool Island::setCaptureActivity(CaptureActivity const& activity) {
    const auto now = Clock::now();
    const std::wstring_view apps[]{activity.screen, activity.camera, activity.microphone};
    bool changed = captureRecording_ != activity.recording;
    captureRecording_ = activity.recording;
    for (size_t i = 0; i < captureIndicators_.size(); ++i) {
        auto& item = captureIndicators_[i];
        if (item.app == apps[i]) continue;
        item.app = apps[i];
        changed = true;
    }
    if (!changed) return false;
    updateCaptureIndicators(now);
    updatePill(now);
    return true;
}

void Island::updateCaptureIndicators(Clock::time_point now) {
    for (auto& item : captureIndicators_) {
        const bool shown = !item.app.empty();
        if (shown == item.shown) continue;
        item.from = captureProgress(now, item);
        item.shown = shown;
        item.start = now;
    }
}

void Island::setOpen(bool open, Clock::time_point now) {
    openAt_.reset();
    closeAt_.reset();
    if (open_ == open || (open && (suppressed_ || bubbleShowing()))) return;
    const bool lyricsVisible = wantsLyrics();
    const auto motion = panelMotion(now);
    const float shown = motion.value;
    if (open) {
        previewUntil_ = {};
        if (shown <= 0) {
            widthStart_ = {};
            screenFrom_ = screen_;
            screenStart_ = {};
            heightFrom_ = screenHeight(screen_);
        }
    }
    shareTitle_ = media_.hasSession && screen_ == Screen::media && pillMode_ == PillMode::media &&
                  pillTitleVisible_ && pillTrackSize_.height <= pillHeight &&
                  pillAmount(now) >= 1 && pillContent_ == PillMode::media &&
                  !pillLyric_.layout && !previousPillLyric_.layout;
    open_ = open;
    if (!open) setNavExpanded(false, now);
    sourcePickerOpen_ = false;
    prayerPickerOpen_ = false;
    audioPickerOpen_ = false;
    updateLyricVisibility(lyricsVisible, now);
    if (!open) panelPinned_ = false;
    keepHeight_ = 0;
    panelFrom_ = shown;
    panelVelocity_ = motion.velocity;
    panelStart_ = now;
    setDotHover(false, now);
    hovered_ = pressed_ = Hit::none;
    timerRepeatAt_ = {};
    dragging_ = false;
    volumeDragId_.clear();
    buttonWasDown_ = true;
    updatePill(now);
}

void Island::showScreen(Screen screen, Clock::time_point now) {
    if (screen == screen_ || !preferences_.enabled(screen)) return;
    const bool lyricsVisible = wantsLyrics();
    heightFrom_ = panelHeight(now);
    widthFrom_ = panelWidthAt(now);
    keepHeight_ = std::max(keepHeight_, heightFrom_);
    screenFrom_ = screen_;
    navSelectionFrom_ = navSelection(now);
    navSelectionStart_ = animate_ ? now : Clock::time_point{};
    screen_ = screen;
    sourcePickerOpen_ = false;
    prayerPickerOpen_ = false;
    audioPickerOpen_ = false;
    if (screen == Screen::media) preferredPill_ = PillMode::media;
    if (screen == Screen::timers) preferredPill_ = quickView_ ? PillMode::timer : PillMode::focus;
    updateLyricVisibility(lyricsVisible, now);
    screenStart_ = animate_ ? now : Clock::time_point{};
    widthStart_ = screenStart_;
}

void Island::toggleFocus(Clock::time_point now) {
    if (countdownCompletion_.mode == PillMode::focus) countdownCompletion_ = {};
    if (focus_.done()) {
        if (autoCycle_ && breakPhase_) focusRound_ = focusRound_ % focusRounds + 1;
        selectFocusPhase(!breakPhase_);
    }
    if (focus_.running()) retirePausedActivity(PillMode::focus, now);
    else preferredPill_ = PillMode::focus;
    focus_.toggle(now);
}

void Island::setAutoCycle(bool enabled) {
    if (autoCycle_ == enabled) return;
    autoCycle_ = enabled;
    focusRound_ = 1;
    // An idle Break picks up (or drops) the long-break length of the new mode.
    if (breakPhase_) selectFocusPhase(true);
}

int Island::phaseSeconds(bool resting) const {
    if (!resting) return focusSeconds_;
    if (autoCycle_ && focusRound_ == focusRounds)
        return longBreakSeconds_.value_or(breakSeconds_ * longBreakFactor);
    return breakSeconds_;
}

std::wstring Island::phaseName(bool resting) const {
    if (!resting) return L"Focus";
    return autoCycle_ && focusRound_ == focusRounds ? L"Long break" : L"Break";
}

// The end of a Focus or Break countdown. With the automatic cycle on, the next phase starts right away.
void Island::finishFocus(Clock::time_point now, bool fromCompanion) {
    const bool rested = breakPhase_;
    std::wstring next;
    if (autoCycle_) {
        if (rested) focusRound_ = focusRound_ % focusRounds + 1;
        selectFocusPhase(!rested);
        focus_.toggle(now);
        next = phaseName(breakPhase_) + L" · " + timestamp(phaseSeconds(breakPhase_));
    }
    IslandAlert alert{rested ? IslandAlert::Kind::breakDone : IslandAlert::Kind::focusDone,
                      rested ? L"Break done" : L"Focus done", std::move(next), std::nullopt, 3, 2600ms};
    alert.fromCompanion = fromCompanion;
    completeCountdown(now, PillMode::focus, std::move(alert));
}

void Island::completeCountdown(Clock::time_point now, PillMode mode, IslandAlert alert) {
    if (!suppressed_ && !open_ && pillMode_ == mode && alertAllowed(alert.kind)) {
        const auto reveal = animate_ ? countdownCompletionMotion : 0ms;
        countdownCompletion_ = {now, now + reveal + alertHoldTime(alert), mode, std::move(alert.title)};
        return;
    }
    if (alert.fromCompanion) {
        const float width = lastActivityDot_ ? lastActivityDot_->right - lastActivityDot_->left : activityWidth(mode, now);
        alert.dotOrigin = IslandAlert::DotOrigin{width, (width - 15) / 2};
    }
    notify(std::move(alert));
}

void Island::selectTimerView(size_t index) {
    const bool quick = index == 2;
    const bool resting = index == 1;
    if (!quick && resting != breakPhase_) {
        if (focus_.running() || focus_.paused()) return;
        selectFocusPhase(resting);
    }
    quickView_ = quick;
    preferredPill_ = quick ? PillMode::timer : PillMode::focus;
}

void Island::selectFocusPhase(bool resting) {
    if (focus_.running() || focus_.paused()) return;
    breakPhase_ = resting;
    focus_.configure(phaseSeconds(resting));
}

int Island::timerDurationLimit() const {
    if (quickView_ || !breakPhase_) return timerMaxSeconds;
    return autoCycle_ && focusRound_ == focusRounds ? breakMaxSeconds * longBreakFactor : breakMaxSeconds;
}

void Island::setTimerDuration(int seconds) {
    auto& timer = quickView_ ? quick_ : focus_;
    if (timer.running() || timer.paused()) return;
    seconds = std::clamp(seconds, 1, timerDurationLimit());
    if (timer.duration() == seconds && !timer.done()) return;
    const auto now = Clock::now();
    timerDigitsFrom_ = timer.seconds(now);
    timerDigitsTo_ = seconds;
    timerDigitsStart_ = now;
    if (!quickView_) {
        if (!breakPhase_) focusSeconds_ = seconds;
        else if (autoCycle_ && focusRound_ == focusRounds) longBreakSeconds_ = seconds;
        else breakSeconds_ = seconds;
    }
    timer.configure(seconds);
}

void Island::adjustTimer(size_t index, int steps) {
    const auto& timer = quickView_ ? quick_ : focus_;
    const int step = index < 2 ? 60 : 1;
    const int delta = index % 2 == 0 ? -step : step;
    setTimerDuration(timer.duration() + delta * steps);
}

std::wstring Island::timerDurationText() const {
    return timestamp(quickView_ ? quick_.duration() : focus_.duration());
}

bool Island::setTimerDurationText(std::wstring_view text) {
    int minutes = 0, seconds = 0;
    bool colon = false;
    size_t minuteDigits = 0, secondDigits = 0;
    for (const auto digit : text) {
        if (digit == L':' && !colon && minuteDigits > 0) {
            colon = true;
        } else if (digit >= L'0' && digit <= L'9') {
            auto& count = colon ? secondDigits : minuteDigits;
            if (++count > (colon ? 2u : 3u)) return false;
            auto& value = colon ? seconds : minutes;
            value = 10 * value + (digit - L'0');
        } else {
            return false;
        }
    }
    if (minuteDigits == 0 || (colon && (secondDigits == 0 || seconds >= 60))) return false;
    setTimerDuration(minutes * 60 + seconds);
    return true;
}

std::optional<RECT> Island::timerEditorRect() const {
    if (!open_ || screen_ != Screen::timers || !timerEditor_) return std::nullopt;
    const auto area = *timerEditor_;
    return RECT{anchor_.x + std::lround(area.left * scale_), anchor_.y + std::lround(area.top * scale_),
                anchor_.x + std::lround(area.right * scale_), anchor_.y + std::lround(area.bottom * scale_)};
}

void Island::tick() {
    const auto now = Clock::now();
    if (timerRepeatAt_ != Clock::time_point{} && now >= timerRepeatAt_) {
        adjustTimer(pressedIndex_);
        timerRepeated_ = true;
        timerRepeatAt_ = now + 85ms;
    }
    if (open_ && !panelPinned_ && !dropHover_ && !popupOpen_) {
        // Hover hysteresis: entering needs the real pill, but the open panel stays while the pointer
        // is within keepOpenMargin of it. An outside click closes an unpinned panel.
        POINT cursor{};
        GetCursorPos(&cursor);
        const D2D1_POINT_2F point{(cursor.x - anchor_.x) / scale_, (cursor.y - anchor_.y) / scale_};
        const auto panel = panelRect(now);
        if (inside(box(panel.left, panel.top, panel.right - panel.left,
                       screenHeight(screen_) + inlineInset() + navHeightAt(now) - navHeight), point)) {
            keepHeight_ = 0;
        }
        auto keep = panel;
        keep.bottom = std::max(keep.bottom, keep.top + keepHeight_);
        const bool changingTimer = pressed_ == Hit::timerDuration || timerRepeatAt_ != Clock::time_point{};
        if (inside(grow(keep, keepOpenMargin, keepOpenMargin), point) || dragging_ || scratchEditing_ || changingTimer) {
            closeAt_.reset();
        } else if (!closeAt_) {
            closeAt_ = now + closeDelay;
        }
        const int button = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
        const bool down = (GetAsyncKeyState(button) & 0x8000) != 0;
        if (down && !buttonWasDown_ && !inside(panel, point)) setOpen(false, now);
        buttonWasDown_ = down;
    }
    if (openAt_ && now >= *openAt_) {
        if (previewUntil_ != Clock::time_point{}) {
            POINT cursor{};
            GetCursorPos(&cursor);
            const auto point = pt((cursor.x - anchor_.x) / scale_, (cursor.y - anchor_.y) / scale_);
            if (!inside(grow(compactShape(now, 1), 0, 3), point)) openAt_.reset();
        }
        if (openAt_) setOpen(true, now);
    }
    if (!openAt_ && now >= previewUntil_) previewUntil_ = {};
    if (closeAt_ && now >= *closeAt_) setOpen(false, now);
    const auto companion = activityCompanion();
    if (focus_.expire(now)) finishFocus(now, companion == PillMode::focus);
    if (quick_.expire(now)) {
        IslandAlert alert{IslandAlert::Kind::timerDone, L"Timer done", L"", std::nullopt, 3, 2600ms};
        alert.fromCompanion = companion == PillMode::timer;
        completeCountdown(now, PillMode::timer, std::move(alert));
    }
    if (alert_) {
        if (alertInline_) {
            if (now >= alertUntil_) finishAlert(now);
            else if (!open_ && panelValue(now) <= 0) {
                auto alert = std::move(*alert_);
                alert.hold = std::chrono::duration_cast<std::chrono::milliseconds>(alertUntil_ - now);
                startAlert(std::move(alert), now, false);
            }
        } else if (alertTo_ > 0) {
            if (!alertHoldUntil_ && (!animate_ || now - alertStart_ >= splitTime)) {
                alertHoldUntil_ = now + alertHoldTime(*alert_);
            }
            if (alertHoldUntil_ && now >= *alertHoldUntil_) {
                beginAlertClose(now);
            }
        } else if (!animate_ || now - alertStart_ >= splitTime) {
            finishAlert(now);
        }
    }
    const bool completionExpired = countdownCompletion_.until != Clock::time_point{} && now >= countdownCompletion_.until;
    const bool returnToMedia = completionExpired && animate_ && !suppressed_ && !open_ && panelValue(now) <= 0 &&
                               !bubbleShowing() && lastActivityDot_ && pillMode_ == countdownCompletion_.mode &&
                               activityCompanion() == PillMode::media;
    const auto completedPill = returnToMedia ? compactShape(now, pillAmount(now)) : D2D1_RECT_F{};
    if (completionExpired && animate_ && !suppressed_ && !open_ && panelValue(now) <= 0 &&
        pillMode_ == countdownCompletion_.mode && media_.hasSession && !media_.playing &&
        preferences_.enabled(Screen::media) && !focus_.running() && !quick_.running() &&
        !focus_.paused() && !quick_.paused()) {
        pausedMediaRevealAt_ = now + shapeTime + idleBeforePausedMedia;
        compactPausedMediaAfterTimer_ = true;
    }
    updateCaptureIndicators(now);
    updatePill(now);
    if (mediaIntroPending_ && (suppressed_ || activityCompanion() != PillMode::media)) {
        mediaIntroPending_ = false;
    } else if (mediaIntroPending_ && !open_ && panelValue(now) <= 0) {
        mediaIntroPending_ = false;
        notify({IslandAlert::Kind::mediaStarted, title()});
    }
    if (alert_ && alert_->kind == IslandAlert::Kind::mediaStarted && activityCompanion() != PillMode::media)
        finishAlert(now);
    if (returnToMedia && pillMode_ == PillMode::media) {
        activitySwap_ = {now, PillMode::media, countdownCompletion_.mode, completedPill, *lastActivityDot_,
                         countdownCompletion_.label, true, true};
        showScreen(Screen::media, now);
        openAt_.reset();
    }
    if (completionExpired) countdownCompletion_ = {};
    updateTheme(now);
}

bool Island::wantsAudioSpectrum() const {
    if (!animate_ || suppressed_ || !preferences_.showWaveform ||
        !preferences_.enabled(Screen::media) || !media_.playing) return false;
    if (open_) return screen_ == Screen::media;
    return !pillOccluded_ && ((pillMode_ == PillMode::media && !pillLyric_.layout) ||
                             activityCompanion() == PillMode::media);
}

unsigned Island::frameInterval() const {
    const auto now = Clock::now();
    const bool timerRunning = focus_.running() || quick_.running();
    if (animate_ && now - visibilityStart_ < visibilityTime) return 10;
    if (suppressed_) return timerRunning ? 1000 : 0;
    if (moving(now) || openAt_ || closeAt_ || dragging_ || timerRepeatAt_ != Clock::time_point{}) return 10;
    if (wantsAudioSpectrum()) return 33;
    if (now < pausedMediaRevealAt_) return 10;
    if (now < countdownCompletion_.until) {
        const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(countdownCompletion_.until - now);
        return static_cast<unsigned>(std::clamp(remaining, 1ms, 1000ms).count());
    }
    if (wantsPrayerClock() && !alert_) return 1000;
    if (panelPinned() && !alert_) {
        if (screen_ == Screen::media && (media_.playing || (wantsLyrics() && now < lyricFollowAt_) ||
            (animate_ && preferences_.showWaveform && now - pausedAt_ < 350ms))) return 50;
        return screen_ == Screen::system || scratchEditing_ || timerRunning ? 1000 : 0;
    }
    if (open_ || alert_) return 50;
    if (pillOccluded_ && pillMode_ != PillMode::preview) return timerRunning ? 1000 : 0;
    if (pillMode_ == PillMode::preview) return 50;
    if (wantsLyricPill()) return 50;
    if (pillMode_ == PillMode::media) {
        if (media_.playing) return animate_ && preferences_.showWaveform ? 33 : timerRunning ? 1000 : 0;
        if (pillTitleVisible_ && pausedAt_ != Clock::time_point{} && preferences_.compactPause != 3) {
            const auto compactAt = pausedAt_ + std::chrono::seconds(preferences_.pauseSeconds());
            const auto remaining = std::max(1ms, std::chrono::ceil<std::chrono::milliseconds>(compactAt - now));
            return static_cast<unsigned>((timerRunning ? std::min(remaining, 1000ms) : remaining).count());
        }
    }
    return timerRunning ? 1000 : 0;
}

bool Island::wantsPrayerClock() const {
    return open_ && screen_ == Screen::prayer && prayerSchedule_.has_value();
}

bool Island::wantsSystemStats() const {
    return open_ && screen_ == Screen::system && preferences_.enabled(Screen::system);
}

// ---- Input ---------------------------------------------------------------------------------------

D2D1_POINT_2F Island::stagePoint(POINT client) const {
    return {(client.x + origin_.x) / scale_, (client.y + origin_.y) / scale_};
}

std::optional<Island::Target> Island::targetAt(D2D1_POINT_2F point) const {
    for (auto it = targets_.rbegin(); it != targets_.rend(); ++it) {
        if (inside(it->rect, point)) return *it;
    }
    if (!recordPanel_ || !wantsLyrics() || !media_.canSeek || !lyricLayout_ || !lyrics_ ||
        lyrics_->status != LyricsStatus::ready ||
        lyricLayoutRevision_ != lyrics_->revision || !inside(lyricViewport_, point)) return std::nullopt;

    const float y = point.y - lyricViewport_.top + lyricScrollShown_;
    if (y >= lyricContentHeight_) return std::nullopt;
    DWRITE_HIT_TEST_METRICS hit{};
    BOOL trailing = FALSE, onText = FALSE;
    if (FAILED(lyricLayout_->HitTestPoint(point.x - lyricViewport_.left, y, &trailing, &onText, &hit)) ||
        y < hit.top || y >= hit.top + hit.height) return std::nullopt;

    const auto next = std::ranges::upper_bound(lyricRanges_, hit.textPosition, {}, &DWRITE_TEXT_RANGE::startPosition);
    if (next == lyricRanges_.begin() || (next - 1)->length == 0) return std::nullopt;
    return Target{lyricViewport_, Hit::lyricLine, static_cast<size_t>(next - lyricRanges_.begin() - 1)};
}

float Island::timelineFraction(D2D1_POINT_2F point) const {
    return clamp01((point.x - timelineRect_.left) / std::max(1.0f, timelineRect_.right - timelineRect_.left));
}

float Island::volumeFraction(D2D1_POINT_2F point) const {
    return clamp01((point.x - volumeTrack_.left) / std::max(1.0f, volumeTrack_.right - volumeTrack_.left));
}

bool Island::pointerMove(POINT client) {
    const auto point = stagePoint(client);
    if (pressed_ == Hit::timerDuration) {
        const int steps = static_cast<int>((timerDragY_ - point.y) / timerDragStep);
        if (steps == 0) return false;
        timerDragY_ -= static_cast<float>(steps) * timerDragStep;
        timerDragged_ = true;
        adjustTimer(pressedIndex_ * 2 + 1, steps);
        return true;
    }
    if (dragging_ && !volumeDragId_.empty()) {
        const float value = volumeFraction(point);
        if (value != volumeDragValue_) {
            volumeDragValue_ = value;
            IslandCommand command{IslandCommand::Kind::appVolume, value, volumeDragId_};
            liveCommand_ = std::move(command);
        }
        return true;
    }
    if (dragging_) {
        dragFraction_ = timelineFraction(point);
        return true;
    }
    const auto now = Clock::now();
    if (!open_ && !suppressed_ && panelValue(now) > 0 && inside(closingPanel_, point)) {
        setOpen(true, now);
        return true;
    }
    const auto target = targetAt(point);
    const auto hit = target ? target->hit : Hit::none;
    const auto index = target ? target->index : 0;
    if (hit != Hit::privacyBadge) privacyDetailsOpen_ = false;
    if (!open_) {
        if (hit == Hit::pill && !activitySwapActive(now)) {
            if (!openAt_) openAt_ = std::max(now + openDelay, previewUntil_);
        } else {
            openAt_.reset();
        }
    }
    const bool navChanged = setNavExpanded(open_ && inside(navHoverRect_, point), now);
    if (hit == hovered_ && index == hoveredIndex_) return navChanged || openAt_.has_value();
    setDotHover(hit == Hit::activityDot || (hit == Hit::pill && pillMode_ == PillMode::dot), now);
    hovered_ = hit;
    hoveredIndex_ = index;
    return true;
}

bool Island::pointerLeave() {
    openAt_.reset();
    privacyDetailsOpen_ = false;
    wheelRemainder_ = 0;
    const auto now = Clock::now();
    const bool navChanged = setNavExpanded(false, now);
    setDotHover(false, now);
    if (hovered_ == Hit::none) return navChanged;
    hovered_ = Hit::none;
    return true;
}

float Island::lyricScroll(Clock::time_point now) const {
    if (!animate_) return lyricScrollTo_;
    return std::lerp(lyricScrollFrom_, lyricScrollTo_, easeOut(progress(lyricScrollStart_, lyricScrollTime, now)));
}

void Island::moveLyricScroll(float offset, Clock::time_point now) {
    if (offset == lyricScrollTo_) return;
    lyricScrollFrom_ = lyricScroll(now);
    lyricScrollTo_ = offset;
    lyricScrollStart_ = now;
}

void Island::updateLyricVisibility(bool wasVisible, Clock::time_point now) {
    const bool visible = wantsLyrics();
    if (visible == wasVisible) return;
    if (visible) {
        lyricFollowAt_ = now + lyricFollowLeft_;
        lyricFollowLeft_ = {};
    } else {
        lyricFollowLeft_ = std::max(Clock::duration::zero(), lyricFollowAt_ - now);
        lyricScrollFrom_ = lyricScrollTo_ = lyricScrollShown_;
        lyricScrollStart_ = {};
    }
}

bool Island::scroll(POINT client, int delta) {
    const auto point = stagePoint(client);
    if (const auto target = targetAt(point); target && target->hit == Hit::timerDuration) {
        if (timerWheelUnit_ != target->index) timerWheelRemainder_ = 0;
        timerWheelUnit_ = target->index;
        timerWheelRemainder_ += delta;
        const int steps = timerWheelRemainder_ / WHEEL_DELTA;
        timerWheelRemainder_ %= WHEEL_DELTA;
        if (steps != 0) adjustTimer(target->index * 2 + 1, steps);
        return steps != 0;
    }
    timerWheelRemainder_ = 0;
    if (audioPickerOpen_ && inside(audioOutputMenuRect_, point)) {
        if (audioOutputs_.size() <= visibleAudioOutputs) return true;
        audioOutputWheelRemainder_ += delta;
        const int steps = audioOutputWheelRemainder_ / WHEEL_DELTA;
        audioOutputWheelRemainder_ %= WHEEL_DELTA;
        const int limit = static_cast<int>(audioOutputs_.size() - visibleAudioOutputs);
        audioOutputOffset_ = static_cast<size_t>(std::clamp(static_cast<int>(audioOutputOffset_) - steps, 0, limit));
        return true;
    }
    audioOutputWheelRemainder_ = 0;
    if (prayerPickerOpen_) {
        const auto target = targetAt(point);
        if (target && (target->hit == Hit::prayerLocation || target->hit == Hit::prayerLocationMenu)) return true;
    }
    if (sourcePickerOpen_ && inside(sourceMenuRect_, point)) {
        const size_t rows = (media_.sources.size() + sourceColumns - 1) / sourceColumns;
        const int limit = static_cast<int>(rows > 2 ? rows - 2 : 0);
        sourceWheelRemainder_ += delta;
        const int steps = sourceWheelRemainder_ / WHEEL_DELTA;
        sourceWheelRemainder_ %= WHEEL_DELTA;
        const int row = static_cast<int>(sourceOffset_ / sourceColumns);
        sourceOffset_ = static_cast<size_t>(std::clamp(row - steps, 0, limit)) * sourceColumns;
        return true;
    }
    if (open_ && screen_ == Screen::system && appVolumes_.size() > visibleAppRows && inside(appViewport_, point)) {
        wheelRemainder_ = 0;
        appWheelRemainder_ += delta;
        const int steps = appWheelRemainder_ / WHEEL_DELTA;
        appWheelRemainder_ %= WHEEL_DELTA;
        const int limit = static_cast<int>(appVolumes_.size() - visibleAppRows);
        appOffset_ = static_cast<size_t>(std::clamp(static_cast<int>(appOffset_) - steps, 0, limit));
        return steps != 0;
    }
    appWheelRemainder_ = 0;
    if (wantsSystemStats() && bluetoothDevices_.size() > visibleBluetoothRows && inside(bluetoothViewport_, point)) {
        wheelRemainder_ = 0;
        bluetoothWheelRemainder_ += delta;
        const int steps = bluetoothWheelRemainder_ / WHEEL_DELTA;
        bluetoothWheelRemainder_ %= WHEEL_DELTA;
        const int limit = static_cast<int>(bluetoothDevices_.size() - visibleBluetoothRows);
        bluetoothOffset_ = static_cast<size_t>(std::clamp(static_cast<int>(bluetoothOffset_) - steps, 0, limit));
        return steps != 0;
    }
    bluetoothWheelRemainder_ = 0;
    if (open_ && screen_ == Screen::drop && dropItems_.size() > visibleDropRows && inside(dropViewport_, point)) {
        wheelRemainder_ = 0;
        dropWheelRemainder_ += delta;
        const int steps = dropWheelRemainder_ / WHEEL_DELTA;
        dropWheelRemainder_ %= WHEEL_DELTA;
        const int limit = static_cast<int>(dropItems_.size() - visibleDropRows);
        dropOffset_ = static_cast<size_t>(std::clamp(static_cast<int>(dropOffset_) - steps, 0, limit));
        return steps != 0;
    }
    dropWheelRemainder_ = 0;
    if (wantsLyrics() && inside(lyricViewport_, point)) {
        wheelRemainder_ = 0;
        return scrollLyrics(delta);
    }
    if (!scratchEditing_ && scratchEditor_ && scratchScrollLimit_ > 0 && inside(*scratchEditor_, point)) {
        wheelRemainder_ = 0;
        const auto offset = std::clamp(scratchScroll_ - 48.0f * static_cast<float>(delta) / WHEEL_DELTA, 0.0f, scratchScrollLimit_);
        if (offset == scratchScroll_) return false;
        scratchScroll_ = offset;
        return true;
    }

    const auto target = targetAt(point);
    const bool compact = !open_ && target && target->hit == Hit::pill;
    const bool panel = open_ && inside(panelWheel_, point);
    if (!(compact || panel) || delta == 0) {
        wheelRemainder_ = 0;
        return false;
    }
    const auto now = Clock::now();
    if (now - wheelAt_ > 300ms || (wheelRemainder_ != 0 && (wheelRemainder_ > 0) != (delta > 0))) {
        wheelRemainder_ = 0;
    }
    wheelAt_ = now;
    wheelRemainder_ += delta;
    if (std::abs(wheelRemainder_) < WHEEL_DELTA) {
        if (compact) openAt_ = std::max(now + 300ms, previewUntil_);
        return compact;
    }

    const int count = static_cast<int>(screens_.size());
    if (count < 2) return false;
    const auto current = std::ranges::find(screens_, screen_) - screens_.begin();
    const int step = wheelRemainder_ < 0 ? 1 : -1;
    wheelRemainder_ %= WHEEL_DELTA;
    showScreen(screens_[(current + step + count) % count], now);
    if (compact) {
        previewUntil_ = now + featurePreviewTime;
        openAt_ = previewUntil_;
    }
    return true;
}

bool Island::scrollLyrics(int delta) {
    const float limit = lyricContentHeight_ - (lyricViewport_.bottom - lyricViewport_.top);
    if (limit <= 0) return false;
    const auto now = Clock::now();
    lyricFollowAt_ = now + 5s;
    const float distance = 75.0f * delta / WHEEL_DELTA;
    moveLyricScroll(std::clamp(lyricScrollTo_ - distance, 0.0f, limit), now);
    return true;
}

bool Island::pointerDown(POINT client) {
    const auto point = stagePoint(client);
    const auto target = targetAt(point);
    if (sourcePickerOpen_ && (!target || (target->hit != Hit::sourcePicker &&
        target->hit != Hit::source && target->hit != Hit::sourceMenu))) sourcePickerOpen_ = false;
    if (prayerPickerOpen_ && (!target || (target->hit != Hit::prayerLocationPicker &&
        target->hit != Hit::prayerLocation && target->hit != Hit::prayerLocationMenu))) prayerPickerOpen_ = false;
    if (audioPickerOpen_ && (!target || (target->hit != Hit::audioPicker && target->hit != Hit::audioOutputMenu &&
        target->hit != Hit::audioOutputChoice && target->hit != Hit::soundSettings))) audioPickerOpen_ = false;
    if (!target) return false;
    if (target->hit == Hit::pill) {
        setOpen(true, Clock::now());
        return false;
    }
    if (target->hit == Hit::timeline) {
        dragging_ = true;
        dragFraction_ = timelineFraction(point);
        return true;
    }
    if (target->hit == Hit::appVolume && target->index < appVolumes_.size()) {
        dragging_ = true;
        volumeDragId_ = appVolumes_[target->index].id;
        volumeTrack_ = grow(target->rect, -6, 0);
        volumeDragValue_ = volumeFraction(point);
        liveCommand_ = IslandCommand{IslandCommand::Kind::appVolume, volumeDragValue_, volumeDragId_};
        return true;
    }
    if (target->hit == Hit::lyricLine) {
        lyricScrollFrom_ = lyricScrollTo_ = lyricScrollShown_;
        lyricScrollStart_ = {};
    }
    pressed_ = target->hit;
    pressedIndex_ = target->index;
    if (pressed_ == Hit::timerDuration) {
        timerDragY_ = point.y;
        timerDragged_ = false;
    } else if (pressed_ == Hit::focusAdjust || pressed_ == Hit::quickAdjust) {
        timerRepeated_ = false;
        timerRepeatAt_ = Clock::now() + 360ms;
    }
    return true;
}

IslandCommand Island::pointerUp(POINT client) {
    const auto now = Clock::now();
    IslandCommand command;
    if (dragging_ && !volumeDragId_.empty()) {
        dragging_ = false;
        command = {IslandCommand::Kind::appVolume, volumeFraction(stagePoint(client)), std::move(volumeDragId_)};
        volumeDragId_.clear();
        liveCommand_.reset();
        return command;
    }
    if (dragging_) {
        dragging_ = false;
        dragFraction_ = timelineFraction(stagePoint(client));
        command.kind = IslandCommand::Kind::seek;
        command.seconds = dragFraction_ * media_.duration;
        return command;
    }
    const auto target = targetAt(stagePoint(client));
    const auto hit = pressed_;
    const auto index = pressedIndex_;
    pressed_ = Hit::none;
    timerRepeatAt_ = {};
    if (!target || target->hit != hit || target->index != index) return command;
    switch (hit) {
    case Hit::activityDot: swapActivities(now); break;
    case Hit::privacyBadge: privacyDetailsOpen_ = !privacyDetailsOpen_; break;
    case Hit::previous: command.kind = IslandCommand::Kind::previous; break;
    case Hit::toggle: command.kind = IslandCommand::Kind::toggle; break;
    case Hit::next: command.kind = IslandCommand::Kind::next; break;
    case Hit::cover: command.kind = IslandCommand::Kind::openSource; break;
    case Hit::sourcePicker:
        sourcePickerOpen_ = !sourcePickerOpen_;
        sourceOffset_ = 0;
        sourceWheelRemainder_ = 0;
        break;
    case Hit::source:
        sourcePickerOpen_ = false;
        if (index < media_.sources.size()) {
            command.kind = IslandCommand::Kind::selectSource;
            const auto& source = media_.sources[index];
            command.appId = source.id.empty() ? source.appId : source.id;
        }
        break;
    case Hit::prayerLocationPicker:
        prayerPickerOpen_ = !prayerPickerOpen_;
        break;
    case Hit::prayerLocation:
        prayerPickerOpen_ = false;
        if (index != preferences_.prayerCity) {
            command.kind = IslandCommand::Kind::prayerLocation;
            command.index = index;
        }
        break;
    case Hit::navMedia: showScreen(Screen::media, now); break;
    case Hit::navTimers: showScreen(Screen::timers, now); break;
    case Hit::navScratch: showScreen(Screen::scratch, now); break;
    case Hit::navDrop: showScreen(Screen::drop, now); break;
    case Hit::navSystem: showScreen(Screen::system, now); break;
    case Hit::navPrayer: showScreen(Screen::prayer, now); break;
    case Hit::lyrics: {
        const bool lyricsVisible = wantsLyrics();
        widthFrom_ = panelWidthAt(now);
        widthStart_ = animate_ ? now : Clock::time_point{};
        lyricsOpen_ = !lyricsOpen_;
        updateLyricVisibility(lyricsVisible, now);
        break;
    }
    case Hit::lyricLine:
        command.kind = IslandCommand::Kind::seek;
        command.seconds = std::clamp(lyrics_->lines[index].seconds + lyrics_->delaySeconds(), 0.0, media_.duration);
        lyricFollowAt_ = now + 500ms;
        break;
    case Hit::retryLyrics: command.kind = IslandCommand::Kind::retryLyrics; break;
    case Hit::lyricsDelay:
        command.kind = IslandCommand::Kind::adjustLyricsDelay;
        command.index = index;
        lyricFollowAt_ = {};
        break;
    case Hit::scratchEdit: command.kind = IslandCommand::Kind::editScratchpad; break;
    case Hit::panelPin:
        panelPinned_ = !panelPinned_;
        closeAt_.reset();
        break;
    case Hit::focusToggle: toggleFocus(now); break;
    case Hit::focusReset:
        if (countdownCompletion_.mode == PillMode::focus) countdownCompletion_ = {};
        focus_.reset();
        if (autoCycle_) {
            focusRound_ = 1;
            selectFocusPhase(false);
        }
        break;
    case Hit::autoCycle:
        setAutoCycle(!autoCycle_);
        command.kind = IslandCommand::Kind::autoCycle;
        command.seconds = autoCycle_ ? 1 : 0;
        break;
    case Hit::timerView: selectTimerView(index); break;
    case Hit::focusAdjust:
    case Hit::quickAdjust:
        if (!timerRepeated_) adjustTimer(index);
        break;
    case Hit::timerDuration:
        if (!timerDragged_) command.kind = IslandCommand::Kind::editTimerDuration;
        break;
    case Hit::timerPreset:
        if (index < std::size(timerPresets) && !quick_.running() && !quick_.paused()) {
            setTimerDuration(timerPresets[index] * 60);
        }
        break;
    case Hit::timerToggle:
        if (countdownCompletion_.mode == PillMode::timer) countdownCompletion_ = {};
        if (quick_.running()) retirePausedActivity(PillMode::timer, now);
        else preferredPill_ = PillMode::timer;
        quick_.toggle(now);
        break;
    case Hit::timerReset:
        if (countdownCompletion_.mode == PillMode::timer) countdownCompletion_ = {};
        quick_.reset();
        break;
    case Hit::audioPicker:
        audioPickerOpen_ = !audioPickerOpen_;
        audioOutputWheelRemainder_ = 0;
        if (audioPickerOpen_) {
            const auto active = std::ranges::find(audioOutputs_, true, &AudioOutput::active);
            const size_t offset = active == audioOutputs_.end() ? 0 : static_cast<size_t>(active - audioOutputs_.begin());
            audioOutputOffset_ = std::min(offset, audioOutputs_.size() - std::min(audioOutputs_.size(), visibleAudioOutputs));
        }
        break;
    case Hit::audioOutputChoice:
        audioPickerOpen_ = false;
        if (index < audioOutputs_.size() && !audioOutputs_[index].active) {
            command.kind = IslandCommand::Kind::selectAudioOutput;
            command.appId = audioOutputs_[index].id;
        }
        break;
    case Hit::soundSettings:
        audioPickerOpen_ = false;
        command.kind = IslandCommand::Kind::soundSettings;
        break;
    case Hit::clearDrop: command.kind = IslandCommand::Kind::clearDrop; break;
    case Hit::dropImage:
        command.kind = IslandCommand::Kind::openDropImage;
        command.index = index;
        break;
    case Hit::removeDrop:
        command.kind = IslandCommand::Kind::removeDrop;
        command.index = index;
        break;
    case Hit::notificationSettings: command.kind = IslandCommand::Kind::notificationSettings; break;
    case Hit::appMute:
        if (index < appVolumes_.size()) {
            command.kind = IslandCommand::Kind::appMute;
            command.appId = appVolumes_[index].id;
            command.seconds = appVolumes_[index].muted ? 0 : 1;
        }
        break;
    default: break;
    }
    return command;
}

void Island::pointerCancel() {
    timerRepeatAt_ = {};
    pressed_ = Hit::none;
    dragging_ = false;
    volumeDragId_.clear();
}

bool Island::overControl() const {
    return hovered_ != Hit::none;
}

void Island::setDotHover(bool hot, Clock::time_point now) {
    if (hot == dotHot_) return;
    dotHoverFrom_ = dotHover(now);
    dotHot_ = hot;
    dotHoverStart_ = now;
}

float Island::dotHover(Clock::time_point now) const {
    const float target = dotHot_ ? 1.0f : 0.0f;
    if (!animate_) return target;
    return std::lerp(dotHoverFrom_, target, easeInOut(progress(dotHoverStart_, dotHoverTime, now)));
}

bool Island::hovered(Hit hit, size_t index) const {
    return hovered_ == hit && hoveredIndex_ == index && pressed_ == Hit::none;
}

bool Island::pressed(Hit hit, size_t index) const {
    return pressed_ == hit && pressedIndex_ == index;
}

void Island::addTarget(D2D1_RECT_F rect, Hit hit, size_t index) {
    targets_.push_back({rect, hit, index});
}

// ---- Device resources ----------------------------------------------------------------------------

void Island::prepareSurface(int width, int height, bool animating) {
    if (!surface_) {
        surface_ = CreateCompatibleDC(nullptr);
        if (!surface_) winrt::throw_last_error();
    }
    if (width > bitmapSize_.cx || height > bitmapSize_.cy ||
        (!animating && (bitmapSize_.cx != width || bitmapSize_.cy != height))) {
        // Leave room for the next frame during a resize, then reclaim it when the shape settles.
        constexpr LONG block = 64;
        const LONG bitmapWidth = animating ? std::max(bitmapSize_.cx, (width + block - 1) / block * block) : width;
        const LONG bitmapHeight = animating ? std::max(bitmapSize_.cy, (height + block - 1) / block * block) : height;
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = bitmapWidth;
        info.bmiHeader.biHeight = -bitmapHeight;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* pixels = nullptr;
        const auto next = CreateDIBSection(surface_, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!next) winrt::throw_last_error();
        const auto previous = SelectObject(surface_, next);
        if (!originalBitmap_) originalBitmap_ = previous;
        if (bitmap_) DeleteObject(bitmap_);
        bitmap_ = next;
        bitmapSize_ = {bitmapWidth, bitmapHeight};
    }
    surfaceSize_ = {width, height};
    if (!graphics_) {
        winrt::check_hresult(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, graphics_.put()));
        const auto round = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                                                       D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND);
        winrt::check_hresult(graphics_->CreateStrokeStyle(round, nullptr, 0, roundStroke_.put()));
        auto dashed = D2D1::StrokeStyleProperties();
        dashed.dashStyle = D2D1_DASH_STYLE_CUSTOM;
        constexpr float dashes[]{4, 4};
        winrt::check_hresult(graphics_->CreateStrokeStyle(dashed, dashes, std::size(dashes), dropDash_.put()));
    }
    if (!target_) {
        const auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
        winrt::check_hresult(graphics_->CreateDCRenderTarget(&properties, target_.put()));
        target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        winrt::check_hresult(target_->CreateSolidColorBrush(rgb(0xffffff), brush_.put()));
    }
}

void Island::releaseDeviceResources() {
    lyricLayout_ = nullptr;
    lyricActiveInk_ = nullptr;
    brush_ = nullptr;
    target_ = nullptr;
    for (auto& layer : layers_) layer = nullptr;
    cover_.bitmap = nullptr;
    dropBitmaps_.clear();
    for (auto& [id, image] : icons_) image.bitmap = nullptr;
}

void Island::prepareText() {
    if (titleFont_) return;
    if (!fonts_) {
        winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(fonts_.put())));
    }
    const auto create = [this](winrt::com_ptr<IDWriteTextFormat>& font, wchar_t const* family, float size,
                               DWRITE_FONT_WEIGHT weight, DWRITE_TEXT_ALIGNMENT alignment) {
        winrt::check_hresult(fonts_->CreateTextFormat(family, nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, size, L"", font.put()));
        font->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        font->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        font->SetTextAlignment(alignment);
        winrt::com_ptr<IDWriteInlineObject> ellipsis;
        winrt::check_hresult(fonts_->CreateEllipsisTrimmingSign(font.get(), ellipsis.put()));
        const DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
        font->SetTrimming(&trim, ellipsis.get());
        ellipses_.push_back(ellipsis);
    };
    constexpr auto normal = DWRITE_FONT_WEIGHT_NORMAL;
    constexpr auto strong = DWRITE_FONT_WEIGHT_SEMI_BOLD;
    constexpr auto start = DWRITE_TEXT_ALIGNMENT_LEADING;
    constexpr auto end = DWRITE_TEXT_ALIGNMENT_TRAILING;
    constexpr auto center = DWRITE_TEXT_ALIGNMENT_CENTER;
    create(pillTitleFont_, L"Segoe UI", 12, strong, start);
    create(timerFont_, L"Consolas", 12, strong, end);
    timerDigitWidth_ = measure(L"0", timerFont_.get());
    create(titleFont_, L"Segoe UI", 16, strong, start);
    create(mediaTitleFont_, L"Segoe UI", mediaTitleSize, strong, start);
    create(prayerTitleFont_, L"Segoe UI", 24, strong, start);
    create(prayerTimeFont_, L"Segoe UI", 13, strong, center);
    create(headingCenterFont_, L"Segoe UI", 16, strong, center);
    create(bodyFont_, L"Segoe UI", 12, normal, start);
    winrt::com_ptr<IDWriteFontCollection> systemFonts;
    winrt::check_hresult(fonts_->GetSystemFontCollection(systemFonts.put()));
    UINT32 fontIndex = 0;
    BOOL variableFont = FALSE;
    winrt::check_hresult(systemFonts->FindFamilyName(L"Segoe UI Variable Text", &fontIndex, &variableFont));
    const auto lyricFamily = variableFont ? L"Segoe UI Variable Text" : L"Segoe UI";
    winrt::check_hresult(fonts_->CreateTextFormat(lyricFamily, systemFonts.get(), DWRITE_FONT_WEIGHT_MEDIUM,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 17, L"", lyricFont_.put()));
    winrt::check_hresult(lyricFont_->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP));
    winrt::check_hresult(lyricFont_->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, 24, 18));
    winrt::check_hresult(fonts_->CreateTextFormat(lyricFamily, systemFonts.get(), DWRITE_FONT_WEIGHT_MEDIUM,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 15, L"", pillLyricFont_.put()));
    winrt::check_hresult(pillLyricFont_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));
    winrt::check_hresult(pillLyricFont_->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP));
    winrt::check_hresult(fonts_->CreateTextFormat(L"Segoe UI", nullptr, normal, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 14, L"", scratchFont_.put()));
    create(bodyEndFont_, L"Segoe UI", 12, normal, end);
    create(strongEndFont_, L"Segoe UI", 12, DWRITE_FONT_WEIGHT_BOLD, end);
    create(monoFont_, L"Consolas", 10, normal, start);
    create(monoEndFont_, L"Consolas", 10, normal, end);
    create(clockFont_, L"Segoe UI", 32, normal, center);
    create(buttonFont_, L"Segoe UI", 11, normal, center);
    create(glyphFont_, L"Segoe UI Symbol", 17, normal, center);
    create(letterFont_, L"Segoe UI", 10, strong, center);
    create(alertTitleFont_, L"Segoe UI", 11, DWRITE_FONT_WEIGHT_BOLD, start);
    create(alertValueFont_, L"Segoe UI", 11, normal, start);
}

ID2D1SolidColorBrush* Island::ink(D2D1_COLOR_F color) {
    brush_->SetColor(color);
    return brush_.get();
}

ID2D1Bitmap* Island::bitmap(Image& image) {
    if (image.pixels.empty()) return nullptr;
    if (!image.bitmap) {
        const auto properties = D2D1::BitmapProperties(
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        if (FAILED(target_->CreateBitmap(D2D1::SizeU(image.width, image.height), image.pixels.data(),
                                         image.width * 4, properties, image.bitmap.put()))) {
            return nullptr;
        }
    }
    return image.bitmap.get();
}

Island::Image* Island::icon(std::wstring const& appId) {
    if (appId.empty()) return nullptr;
    auto [entry, added] = icons_.try_emplace(appId);
    if (added && !loadIcon(appId, 48, entry->second.pixels, entry->second.width, entry->second.height)) {
        entry->second.pixels.clear();
    }
    return entry->second.pixels.empty() ? nullptr : &entry->second;
}

void Island::pushLayer(ID2D1Geometry* mask, float opacity) {
    auto& layer = layers_.at(layerDepth_++);
    if (!layer) winrt::check_hresult(target_->CreateLayer(nullptr, layer.put()));
    target_->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), mask, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                             D2D1::IdentityMatrix(), opacity), layer.get());
}

void Island::popLayer() {
    target_->PopLayer();
    --layerDepth_;
}

// ---- Drawing primitives --------------------------------------------------------------------------

void Island::text(std::wstring_view value, IDWriteTextFormat* format, D2D1_RECT_F rect, D2D1_COLOR_F color) {
    if (!value.empty()) {
        target_->DrawText(value.data(), static_cast<UINT32>(value.size()), format, rect, ink(color),
                          D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
}

// Black-dominant glass; the cover only tints the far corner of the gradient.
void Island::fillShell(D2D1_ROUNDED_RECT shape) {
    const D2D1_GRADIENT_STOP stops[]{{0, D2D1::ColorF(0, 0, 0, 0.94f)}, {1, withAlpha(tint_, 0.9f)}};
    winrt::com_ptr<ID2D1GradientStopCollection> collection;
    winrt::check_hresult(target_->CreateGradientStopCollection(stops, 2, collection.put()));
    winrt::com_ptr<ID2D1LinearGradientBrush> gradient;
    winrt::check_hresult(target_->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties(D2D1::Point2F(shape.rect.left, shape.rect.top),
                                            D2D1::Point2F(shape.rect.right, shape.rect.bottom)),
        collection.get(), gradient.put()));
    target_->FillRoundedRectangle(shape, gradient.get());
}

void Island::fillShell(ID2D1Geometry* shape) {
    D2D1_RECT_F bounds{};
    winrt::check_hresult(shape->GetBounds(nullptr, &bounds));
    const D2D1_GRADIENT_STOP stops[]{{0, D2D1::ColorF(0, 0, 0, 0.94f)}, {1, withAlpha(tint_, 0.9f)}};
    winrt::com_ptr<ID2D1GradientStopCollection> collection;
    winrt::check_hresult(target_->CreateGradientStopCollection(stops, 2, collection.put()));
    winrt::com_ptr<ID2D1LinearGradientBrush> gradient;
    winrt::check_hresult(target_->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties(D2D1::Point2F(bounds.left, bounds.top),
                                            D2D1::Point2F(bounds.right, bounds.bottom)),
        collection.get(), gradient.put()));
    target_->FillGeometry(shape, gradient.get());
}

void Island::fillPolygon(std::initializer_list<D2D1_POINT_2F> points, D2D1_COLOR_F color) {
    winrt::com_ptr<ID2D1PathGeometry> path;
    winrt::check_hresult(graphics_->CreatePathGeometry(path.put()));
    winrt::com_ptr<ID2D1GeometrySink> sink;
    winrt::check_hresult(path->Open(sink.put()));
    auto point = points.begin();
    sink->BeginFigure(*point, D2D1_FIGURE_BEGIN_FILLED);
    for (++point; point != points.end(); ++point) sink->AddLine(*point);
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    winrt::check_hresult(sink->Close());
    target_->FillGeometry(path.get(), ink(color));
}

void Island::drawFocusGlyph(D2D1_POINT_2F center, float size, D2D1_COLOR_F color) {
    const float s = size / 16;
    const float stroke = 1.5f * s;
    const auto at = [&](float x, float y) { return D2D1::Point2F(center.x + (x - 8) * s, center.y + (y - 8) * s); };
    target_->DrawEllipse(D2D1::Ellipse(center, 6.25f * s, 6.25f * s), ink(color), stroke);
    target_->DrawLine(at(8, 4.5f), at(8, 8), ink(color), stroke, roundStroke_.get());
    target_->DrawLine(at(8, 8), at(10.5f, 9.5f), ink(color), stroke, roundStroke_.get());
}

void Island::drawTimerGlyph(D2D1_POINT_2F center, D2D1_COLOR_F color) {
    drawFocusGlyph(pt(center.x, center.y + 1.5f), 14, color);
    target_->DrawLine(pt(center.x - 2.5f, center.y - 7.5f), pt(center.x + 2.5f, center.y - 7.5f),
                      ink(color), 1.4f, roundStroke_.get());
    target_->DrawLine(pt(center.x, center.y - 7.5f), pt(center.x, center.y - 4.5f),
                      ink(color), 1.4f, roundStroke_.get());
}

void Island::drawChipGlyph(D2D1_POINT_2F center, D2D1_COLOR_F color) {
    target_->DrawRoundedRectangle(rounded(box(center.x - 5, center.y - 5, 10, 10), 2), ink(color), 1.2f);
    for (const float offset : {-2.5f, 2.5f}) {
        target_->DrawLine({center.x + offset, center.y - 8}, {center.x + offset, center.y - 6}, ink(color), 1.2f);
        target_->DrawLine({center.x + offset, center.y + 6}, {center.x + offset, center.y + 8}, ink(color), 1.2f);
        target_->DrawLine({center.x - 8, center.y + offset}, {center.x - 6, center.y + offset}, ink(color), 1.2f);
        target_->DrawLine({center.x + 6, center.y + offset}, {center.x + 8, center.y + offset}, ink(color), 1.2f);
    }
}

void Island::drawButton(D2D1_RECT_F rect, std::wstring_view label, Hit hit, bool record, size_t index, bool selected) {
    const float fill = pressed(hit, index) ? 0.14f : hovered(hit, index) ? 0.08f : 0.04f;
    target_->FillRoundedRectangle(rounded(rect, 8), ink(selected ? withAlpha(accent_, 0.16f) : white(fill)));
    target_->DrawRoundedRectangle(rounded(grow(rect, -0.5f, -0.5f), 7.5f), ink(white(0.12f)), 1);
    text(label, buttonFont_.get(), rect, selected ? accent_ : rgb(inkColor));
    if (record) addTarget(rect, hit, index);
}

D2D1_RECT_F Island::pillArtRect(D2D1_RECT_F pill) const {
    const float available = pill.right - pill.left;
    const float width = std::min(available, std::max(pillWidth, pillTrackSize_.width));
    const float reveal = lin(dotSize, pausedPillWidth, width);
    const float full = lin(pausedPillWidth, pillWidth, width);
    const float size = std::lerp(8.0f, std::lerp(16.0f, 20.0f, full), reveal);
    const float center = (pill.left + pill.right) / 2;
    const float left = center - width / 2 + std::lerp(7.0f, 8.0f, full);
    return box(std::lerp(center - size / 2, left, reveal), (pill.top + pill.bottom - size) / 2, size, size);
}

void Island::drawArt(D2D1_RECT_F rect, float opacity) {
    if (opacity <= 0) return;
    const float size = rect.right - rect.left;
    const float radius = std::lerp(size / 2, std::min(8.0f, size * 0.24f), lin(8, 16, size));
    if (auto* image = bitmap(cover_)) {
        winrt::com_ptr<ID2D1BitmapBrush> fill;
        winrt::check_hresult(target_->CreateBitmapBrush(image, fill.put()));
        fill->SetInterpolationMode(D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        fill->SetOpacity(opacity);
        fill->SetTransform(D2D1::Matrix3x2F::Scale(size / cover_.width, (rect.bottom - rect.top) / cover_.height) *
                           D2D1::Matrix3x2F::Translation(rect.left, rect.top));
        target_->FillRoundedRectangle(rounded(rect, radius), fill.get());
        return;
    }
    target_->FillRoundedRectangle(rounded(rect, radius), ink(white(0.08f * opacity)));
    const auto center = pt((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
    const float unit = std::min(1.25f, size / 16);
    const auto color = ink(rgb(mutedColor, opacity));
    target_->FillEllipse(D2D1::Ellipse(pt(center.x - 2 * unit, center.y + 3 * unit), 2.2f * unit, 1.6f * unit), color);
    target_->DrawLine(pt(center.x, center.y + 3 * unit), pt(center.x, center.y - 4 * unit), color, 1.25f * unit,
                      roundStroke_.get());
    target_->DrawLine(pt(center.x, center.y - 4 * unit), pt(center.x + 3 * unit, center.y - 2 * unit), color,
                      1.25f * unit, roundStroke_.get());
}

// ---- Compact: dot and pill -----------------------------------------------------------------------

static D2D1_RECT_F compactHit(D2D1_RECT_F shape, float amount, bool occluded) {
    if (occluded) return shape;
    return amount >= 1 ? grow(shape, 0, 3) : box(-22, pillTop - 3, 44, 46);
}

void Island::drawCompactContent(Clock::time_point now, D2D1_RECT_F shape, bool hideTitle, bool hideArt) {
    if (pillContent_ == PillMode::privacy) {
        drawCenteredCaptureIndicators(shape, captureAmounts(now));
        return;
    }
    const auto badge = privacyBadgeRect(now, shape);
    const float badgeWidth = badge.right - badge.left;
    if (badgeWidth > 0.01f && shape.right - shape.left >= pausedPillWidth + badgeWidth - 1) {
        drawPrivacyBadge(now, badge);
        shape.right -= badgeWidth;
    }
    const auto center = pt((shape.left + shape.right) / 2, (shape.top + shape.bottom) / 2);
    const float t = pillTransitionProgress(now);
    const float fadeOut = pillLyricOut(t);
    const float oldOpacity = previousPillAlpha_ * (1 - fadeOut);
    const float newOpacity = pillLyricIn(t);
    const auto drawLyric = [&](PillText const& lyric, float opacity, float offset) {
        if (!lyric.layout || opacity <= 0) return;
        target_->DrawTextLayout(pt(center.x + (pillLyricIconSpace - lyric.textSize.width) / 2,
                                   center.y - lyric.textSize.height / 2 + offset),
                                lyric.layout.get(), ink(rgb(inkColor, opacity)));
    };
    drawLyric(previousPillLyric_, oldOpacity, std::lerp(previousPillOffset_, -pillLyricTravel, fadeOut));
    drawLyric(pillLyric_, newOpacity, pillLyricTravel * (1 - newOpacity));
    const bool bothLyrics = previousPillLyric_.layout && pillLyric_.layout;
    const float iconOpacity = bothLyrics ? 1 : (pillLyric_.layout ? newOpacity : (previousPillLyric_.layout ? oldOpacity : 0));
    if (iconOpacity > 0) {
        drawLyricsGlyph(pt(shape.left + 13, center.y - 8), withAlpha(accent_, iconOpacity));
    }
    float mediaOpacity = 1;
    if (previousPillLyric_.layout || pillLyric_.layout) {
        mediaOpacity = (previousPillLyric_.layout ? 0 : oldOpacity) + (pillLyric_.layout ? 0 : newOpacity);
    }
    if (mediaOpacity > 0) {
        if (pillContent_ == PillMode::media && !hideArt) drawArt(pillArtRect(shape), mediaOpacity);
        target_->SetTransform(D2D1::Matrix3x2F::Translation(center.x, center.y - pillCenterY) * base_);
        if (mediaOpacity < 1) pushLayer(nullptr, mediaOpacity);
        drawPillContent(now, shape.right - shape.left, hideTitle);
        if (mediaOpacity < 1) popLayer();
        target_->SetTransform(base_);
    }
}

void Island::drawCaptureIndicators(D2D1_POINT_2F right, std::array<float, 3> const& amounts) {
    for (size_t i = 0; i < amounts.size(); ++i) {
        const auto& item = captureIndicators_[i];
        const auto amount = amounts[i];
        const float opacity = lin(0.60f, 1, amount);
        if (opacity > 0) {
            drawAlertIcon(item.kind, pt(right.x - 8 + 4 * (1 - opacity), right.y - 8), rgb(item.color, opacity));
        }
        right.x -= captureSlot * amount;
    }
}

void Island::drawCenteredCaptureIndicators(D2D1_RECT_F shape, std::array<float, 3> const& amounts) {
    const auto [screen, camera, mic] = amounts;
    const auto right = pt((shape.left + shape.right + captureSlot * (screen + camera + mic -
                           std::max({screen, camera, mic}))) / 2,
                          (shape.top + shape.bottom) / 2);
    drawCaptureIndicators(right, amounts);
}

float Island::companionSlotOpacity(Clock::time_point now) const {
    if (!bubbleShowing()) return 1;
    return 1 - easeInOut(lin(0, 0.22f, alertValue(now)));
}

void Island::drawPrivacyBadge(Clock::time_point now, D2D1_RECT_F badge) {
    const auto amounts = captureAmounts(now);
    const float presence = *std::ranges::max_element(amounts);
    if (presence <= 0.01f) return;
    target_->FillRoundedRectangle(rounded(badge, 9), ink(rgb(0x6cdda0, 0.10f * presence)));
    constexpr float iconSize = 14;
    const float middle = (badge.top + badge.bottom) / 2;
    float x = badge.left + 7 * presence;
    for (const auto i : privacyIconOrder) {
        const float slot = privacyIconPitch * amounts[i];
        const float opacity = lin(0.5f, 1, amounts[i]);
        if (opacity > 0) {
            const auto& indicator = captureIndicators_[i];
            drawAlertIcon(indicator.kind, pt(x + (slot - iconSize) / 2, middle - iconSize / 2),
                          rgb(indicator.color, opacity), iconSize);
        }
        x += slot;
    }
}

void Island::drawPrivacyDetails(D2D1_RECT_F details) {
    const auto shell = rounded(details, 9);
    target_->FillRoundedRectangle(shell, ink(rgb(0x13171a)));
    target_->DrawRoundedRectangle(shell, ink(white(0.18f)), 1);
    float top = details.top + 6;
    for (const auto i : privacyIconOrder) {
        const auto& indicator = captureIndicators_[i];
        if (!indicator.shown) continue;
        drawAlertIcon(indicator.kind, pt(details.left + 9, top + 4), rgb(indicator.color), 13);
        std::wstring label = privacyLabels[i];
        if (i == 0 && captureRecording_) label = L"Recording";
        if (!indicator.app.empty()) label += L" · " + indicator.app;
        text(label, alertTitleFont_.get(), box(details.left + 28, top, details.right - details.left - 38, 21),
             rgb(inkColor));
        top += 22;
    }
}

void Island::drawCompanionShell(D2D1_RECT_F shape, float pinch, float border) {
    const float height = shape.bottom - shape.top;
    if (height <= 0.1f) return;
    if (pinch <= 0) {
        const auto shell = rounded(shape, height / 2);
        fillShell(shell);
        target_->DrawRoundedRectangle(shell, ink(white(border)), 1);
        return;
    }
    const float scale = height / pillHeight;
    PathBuilder path(graphics_.get());
    releasedPath(path, 0, 0, 1 - pinch, 0, (shape.right - shape.left) / scale, false);
    const auto transform = D2D1::Matrix3x2F::Scale(scale, scale) *
        D2D1::Matrix3x2F::Translation(shape.left, shape.top - pillTop * scale);
    winrt::com_ptr<ID2D1TransformedGeometry> geometry;
    winrt::check_hresult(graphics_->CreateTransformedGeometry(path.finish().get(), &transform, geometry.put()));
    fillShell(geometry.get());
    target_->DrawGeometry(geometry.get(), ink(white(border)), 1);
}

void Island::drawLyricsGlyph(D2D1_POINT_2F origin, D2D1_COLOR_F color) {
    const auto x = origin.x, y = origin.y;
    target_->FillEllipse(D2D1::Ellipse(pt(x + 3.5f, y + 12), 2.7f, 1.9f), ink(color));
    target_->DrawLine(pt(x + 5.6f, y + 12), pt(x + 5.6f, y + 2.5f), ink(color), 1.4f, roundStroke_.get());
    target_->DrawLine(pt(x + 5.6f, y + 2.5f), pt(x + 10, y + 4), ink(color), 1.4f, roundStroke_.get());
    target_->DrawLine(pt(x + 9, y + 8), pt(x + 15, y + 8), ink(color), 1.4f, roundStroke_.get());
    target_->DrawLine(pt(x + 9, y + 12), pt(x + 13.5f, y + 12), ink(color), 1.4f, roundStroke_.get());
}

void Island::drawPillContent(Clock::time_point now, float width, bool hideTitle) {
    const float left = -width / 2;
    const float right = width / 2 - 12;
    if (pillContent_ == PillMode::preview) {
        drawScreenGlyph(screen_, box(pillLeft + 8, pillTop, 26, pillHeight), accent_);
        text(pillTitle_, pillTitleFont_.get(), box(pillLeft + 39, pillTop, right - (pillLeft + 39), pillHeight),
             rgb(inkColor));
        return;
    }
    if (pillContent_ == PillMode::focus || pillContent_ == PillMode::timer) {
        const bool quickTimer = pillContent_ == PillMode::timer;
        const auto& timer = quickTimer ? quick_ : focus_;
        const auto color = timer.paused() ? rgb(mutedColor) : accent_;
        const auto center = pt(left + 19, pillCenterY);
        const bool completed = countdownCompletion_.mode == pillContent_ && now < countdownCompletion_.until;
        const float motion = completed ? (animate_ ? progress(countdownCompletion_.start, countdownCompletionMotion, now) : 1) : 0;
        const float checkOpacity = easeOut(lin(0.04f, 0.58f, motion));
        const auto timerColor = withAlpha(color, 1 - checkOpacity);
        if (checkOpacity < 1) {
            if (timer.paused()) {
                target_->FillRectangle(box(center.x - 4, center.y - 5, 2.5f, 10), ink(timerColor));
                target_->FillRectangle(box(center.x + 1.5f, center.y - 5, 2.5f, 10), ink(timerColor));
            } else if (quickTimer) {
                drawTimerGlyph(center, timerColor);
            } else {
                drawFocusGlyph(center, 14, timerColor);
            }
        }
        if (checkOpacity > 0) {
            const float pop = std::lerp(0.76f, 1.0f, easeOut(motion)) + 0.15f * std::sin(pi * motion);
            drawCountdownCheck(center, withAlpha(accent_, checkOpacity), pop);
        }

        const float labelOpacity = easeInOut(lin(0.26f, 0.95f, motion));
        const auto clock = countdownTime(completed ? 0 : timer.seconds(now));
        const float clockWidth = timerDigitWidth_ * static_cast<float>(clock.size()) + 1;
        if (labelOpacity < 1) {
            text(clock, timerFont_.get(), box(right - clockWidth, pillTop, clockWidth, pillHeight),
                 withAlpha(color, 1 - labelOpacity));
            text(pillTitle_, pillTitleFont_.get(), box(left + 34, pillTop,
                 std::max(0.0f, right - clockWidth - 8 - (left + 34)), pillHeight),
                 withAlpha(rgb(inkColor), 1 - labelOpacity));
        }
        if (labelOpacity > 0)
            text(countdownCompletion_.label, pillTitleFont_.get(), box(left + 34, pillTop,
                 std::max(0.0f, right - (left + 34)), pillHeight), withAlpha(rgb(inkColor), labelOpacity));
        return;
    }
    const float trackWidth = std::clamp(width, pausedPillWidth, std::max(pillWidth, pillTrackSize_.width));
    const float trackLeft = -trackWidth / 2;
    const float trackRight = trackWidth / 2;
    const float full = lin(pausedPillWidth, pillWidth, trackWidth);
    if (pillPlaying_ && preferences_.showWaveform) {
        drawSpectrumBars(pt(trackRight - 20.75f, pillCenterY), 5, 2.5f, 15, accent_);
    } else if (!pillPlaying_) {
        // Paused: the panel's pause glyph (two bars), scaled for the pill.
        const float pauseLeft = trackRight - std::lerp(19.0f, 22.0f, full);
        for (const float x : {pauseLeft, pauseLeft + 5.5f}) {
            target_->FillRoundedRectangle(rounded(box(x, pillCenterY - 5, 2.5f, 10), 1), ink(rgb(mutedColor)));
        }
    }
    if (!hideTitle && pillTrackTitle_.layout) {
        const float opacity = pillTitleOpacity(now);
        if (opacity > 0) {
            target_->DrawTextLayout(pt(trackLeft + pillTrackLeft, pillCenterY - pillTrackTitle_.textSize.height / 2),
                                    pillTrackTitle_.layout.get(), ink(rgb(inkColor, opacity)));
        }
    }
}

// ---- Panel ---------------------------------------------------------------------------------------

void Island::drawSpectrumBars(D2D1_POINT_2F center, float spacing, float barWidth, float maxHeight, D2D1_COLOR_F color) {
    constexpr size_t bars = 4;
    constexpr size_t bandsPerBar = Spectrum{}.size() / bars;
    const float left = center.x - ((bars - 1) * spacing + barWidth) / 2;
    for (size_t bar = 0; bar < bars; ++bar) {
        const auto first = spectrum_.begin() + bar * bandsPerBar;
        const float level = animate_ ? *std::max_element(first, first + bandsPerBar) : 0;
        const float height = std::lerp(barWidth, maxHeight, level);
        target_->FillRoundedRectangle(rounded(box(left + static_cast<float>(bar) * spacing,
                                                  center.y - height / 2, barWidth, height), barWidth / 2), ink(color));
    }
}

void Island::drawCountdownCheck(D2D1_POINT_2F center, D2D1_COLOR_F color, float scale) {
    PathBuilder check(graphics_.get());
    check.move(pt(center.x - 5 * scale, center.y), false)
         .line(pt(center.x - 1.5f * scale, center.y + 3.5f * scale))
         .line(pt(center.x + 5 * scale, center.y - 4 * scale));
    target_->DrawGeometry(check.finish().get(), ink(color), 1.9f, roundStroke_.get());
}

float Island::activityWidth(PillMode mode, Clock::time_point now) const {
    if (mode != PillMode::focus && mode != PillMode::timer) return activityDotSize;
    const auto& timer = mode == PillMode::timer ? quick_ : focus_;
    return std::ceil(timerDigitWidth_ * static_cast<float>(countdownTime(timer.seconds(now)).size())) + 18;
}

void Island::drawActivityIndicator(Clock::time_point now, D2D1_RECT_F shape, PillMode mode, D2D1_COLOR_F color) {
    const auto center = pt((shape.left + shape.right) / 2, (shape.top + shape.bottom) / 2);
    if (mode == PillMode::media && preferences_.showWaveform) {
        drawSpectrumBars(center, 4.5f, 2, 15, accent_);
    } else if (mode == PillMode::media) {
        drawScreenGlyph(Screen::media, shape, color);
    } else if (mode == PillMode::focus || mode == PillMode::timer) {
        const auto& countdown = mode == PillMode::timer ? quick_ : focus_;
        const auto clock = countdownTime(countdown.seconds(now));
        const float width = std::ceil(timerDigitWidth_ * static_cast<float>(clock.size()));
        if (shape.right - shape.left >= width + 4) {
            text(clock, timerFont_.get(), box(center.x - width / 2, shape.top, width, shape.bottom - shape.top),
                 countdown.paused() ? rgb(mutedColor) : color);
        }
    }
}

void Island::drawActivityDot(Clock::time_point now, D2D1_RECT_F shape, PillMode mode, bool interactive) {
    const auto shell = rounded(shape, (shape.bottom - shape.top) / 2);
    fillShell(shell);
    const float hover = interactive ? dotHover(now) : 0;
    target_->DrawRoundedRectangle(shell, ink(white(std::lerp(0.16f, 0.42f, hover))), 1);
    drawActivityIndicator(now, shape, mode, rgb(inkColor));
    if (interactive) {
        const auto hit = grow(shape, 2, 2);
        target_->FillRectangle(hit, ink(white(1.0f / 255)));
        addTarget(hit, Hit::activityDot, static_cast<size_t>(mode));
    }
}

void Island::drawActivitySwap(Clock::time_point now, D2D1_RECT_F pill, D2D1_RECT_F dot) {
    const float t = progress(activitySwap_.start, activitySwapTime, now);
    const float p = t * t * (3 - 2 * t);
    auto incoming = mixRect(activitySwap_.dot, pill, p);
    auto outgoing = mixRect(activitySwap_.pill, dot, p);
    const float arc = std::sin(pi * p);
    const float separation = arc * arc;
    const float lift = std::clamp(incoming.top - 2, 0.0f, activitySwapLift) * separation;
    incoming.top -= lift;
    incoming.bottom -= lift;
    outgoing.top += activitySwapDrop * separation;
    outgoing.bottom += activitySwapDrop * separation;

    const auto outgoingShell = rounded(outgoing, (outgoing.bottom - outgoing.top) / 2);
    fillShell(outgoingShell);
    target_->DrawRoundedRectangle(outgoingShell, ink(white(0.13f)), 1);
    const float outgoingText = 1 - easeInOut(lin(0.06f, 0.45f, p));
    const float iconX = std::lerp((outgoing.left + outgoing.right) / 2, outgoing.left + 18, outgoingText);
    const auto icon = D2D1::RectF(iconX - 9, outgoing.top, iconX + 9, outgoing.bottom);
    if (outgoingText > 0) {
        drawScreenGlyph(activitySwap_.outgoing == PillMode::media ? Screen::media : Screen::timers,
                        icon, withAlpha(rgb(inkColor), outgoingText));
    }
    const float indicator = easeInOut(lin(0.40f, 0.85f, p));
    if (indicator > 0) {
        pushLayer(nullptr, indicator);
        drawActivityIndicator(now, outgoing, activitySwap_.outgoing, rgb(inkColor));
        popLayer();
    }
    if (outgoingText > 0 && outgoing.right - outgoing.left > 40) {
        const float labelLeft = outgoing.left + 34;
        const float labelRight = outgoing.right - 6;
        if (labelRight > labelLeft)
            text(activitySwap_.title, pillTitleFont_.get(), D2D1::RectF(labelLeft, outgoing.top, labelRight, outgoing.bottom),
                 withAlpha(rgb(inkColor), outgoingText));
    }

    const auto shell = rounded(incoming, (incoming.bottom - incoming.top) / 2);
    fillShell(shell);
    target_->DrawRoundedRectangle(shell, ink(white(0.13f)), 1);
    const float content = easeInOut(lin(0.38f, 0.88f, p));
    if (content < 1) {
        pushLayer(nullptr, 1 - content);
        drawActivityIndicator(now, incoming, activitySwap_.incoming, rgb(inkColor));
        popLayer();
    }
    if (content > 0) {
        winrt::com_ptr<ID2D1RoundedRectangleGeometry> clip;
        winrt::check_hresult(graphics_->CreateRoundedRectangleGeometry(shell, clip.put()));
        pushLayer(clip.get(), content);
        drawCompactContent(now, incoming, false);
        popLayer();
    }
}

void Island::drawActivityHandoff(Clock::time_point now, D2D1_RECT_F pill) {
    const auto duration = std::chrono::duration_cast<Clock::duration>(activityHandoffTime);
    const auto elapsed = std::clamp(now - activitySwap_.start, Clock::duration::zero(), duration);
    const auto splitElapsed = elapsed * splitTime.count() / activityHandoffTime.count();
    const float merge = 1 - reverseSplitMotion(activitySwap_.start, activitySwap_.start + splitElapsed).value;
    auto retiring = mixRect(activitySwap_.pill, pill, merge);
    auto incoming = mixRect(activitySwap_.dot, pill, merge);
    const float dotCenter = (activitySwap_.dot.left + activitySwap_.dot.right) / 2;
    const float pillCenter = (pill.left + pill.right) / 2;
    const float originalCenter = (activitySwap_.pill.left + activitySwap_.pill.right) / 2;
    const float slide = 1 - std::pow(1 - merge, 1.75f);
    const float lead = (pillCenter - dotCenter) * (slide - merge);
    incoming.left += lead;
    incoming.right += lead;

    const float follow = std::clamp((dotCenter - originalCenter) * 0.2f, -22.0f, 22.0f) *
                         std::sin(pi * merge) * (1 - merge);
    retiring.left += follow;
    retiring.right += follow;
    const float height = incoming.bottom - incoming.top;

    drawHandoffShell(retiring, incoming, merge);
    drawHandoffOutgoing(retiring, 1 - easeInOut(lin(0.05f, 0.42f, merge)));
    const float content = easeInOut(lin(0.28f, 0.72f, merge));
    if (content < 1) {
        pushLayer(nullptr, 1 - content);
        drawActivityIndicator(now, incoming, activitySwap_.incoming, rgb(inkColor));
        popLayer();
    }
    if (content > 0) {
        const auto shell = rounded(incoming, height / 2);
        winrt::com_ptr<ID2D1RoundedRectangleGeometry> clip;
        winrt::check_hresult(graphics_->CreateRoundedRectangleGeometry(shell, clip.put()));
        pushLayer(clip.get(), content);
        drawCompactContent(now, incoming, false);
        popLayer();
    }
}

void Island::drawHandoffShell(D2D1_RECT_F retiring, D2D1_RECT_F incoming, float merge) {
    const float blend = 14 * std::sin(pi * merge);
    const float gap = std::max(incoming.left - retiring.right, retiring.left - incoming.right);
    if (blend <= 0.01f || gap >= blend / 2) {
        if (merge < 0.999f) drawCompanionShell(retiring, 0, 0.13f);
        drawCompanionShell(incoming, 0, 0.13f);
        return;
    }

    const auto radius = [](D2D1_RECT_F rect) {
        return std::min(rect.right - rect.left, rect.bottom - rect.top) / 2;
    };
    const float oldRadius = radius(retiring);
    const float newRadius = radius(incoming);
    const float centerY = (incoming.top + incoming.bottom) / 2;
    const auto capsuleDistance = [](D2D1_RECT_F rect, float radius, float x, float y) {
        const float axis = std::clamp(x, rect.left + radius, rect.right - radius);
        return std::hypot(x - axis, y) - radius;
    };
    // A smooth union creates one outline and rounds the neck without changing either pill's target bounds.
    const auto surface = [&](float x, float y) {
        const float a = capsuleDistance(retiring, oldRadius, x, y);
        const float b = capsuleDistance(incoming, newRadius, x, y);
        const float influence = std::max(0.0f, 1 - std::abs(a - b) / blend);
        return std::min(a, b) - blend * influence * influence / 4;
    };
    const auto extent = [&](float inner, float outer) {
        for (int i = 0; i < 10; ++i) {
            const float middle = (inner + outer) / 2;
            if (surface(middle, 0) <= 0) inner = middle;
            else outer = middle;
        }
        return inner;
    };
    const float left = std::min(retiring.left, incoming.left);
    const float right = std::max(retiring.right, incoming.right);
    const float xMin = extent(left, left - blend);
    const float xMax = extent(right, right + blend);
    const float maxDepth = std::max(oldRadius, newRadius) + blend;
    constexpr int samples = 96;
    std::array<Point, samples + 1> outline{};
    for (int i = 0; i <= samples; ++i) {
        const float t = (1 - std::cos(pi * static_cast<float>(i) / samples)) / 2;
        const float x = std::lerp(xMin, xMax, t);
        float inside = 0;
        float outside = maxDepth;
        for (int j = 0; j < 10; ++j) {
            const float middle = (inside + outside) / 2;
            if (surface(x, middle) <= 0) inside = middle;
            else outside = middle;
        }
        outline[i] = pt(x, centerY - inside);
    }

    PathBuilder path(graphics_.get());
    path.move(outline.front());
    for (int i = 1; i <= samples; ++i) path.line(outline[i]);
    for (int i = samples; i >= 0; --i) path.line(pt(outline[i].x, 2 * centerY - outline[i].y));
    const auto shape = path.close().finish();
    fillShell(shape.get());
    target_->DrawGeometry(shape.get(), ink(white(0.13f)), 1);
}

void Island::drawHandoffOutgoing(D2D1_RECT_F pill, float opacity) {
    if (opacity <= 0) return;
    const float height = pill.bottom - pill.top;
    const float width = pill.right - pill.left;
    const auto icon = box(pill.left + 9, pill.top, 18, height);
    if (activitySwap_.outgoingCompleted)
        drawCountdownCheck(pt(pill.left + 18, (pill.top + pill.bottom) / 2), withAlpha(accent_, opacity));
    else
        drawScreenGlyph(activitySwap_.outgoing == PillMode::media ? Screen::media : Screen::timers,
                        icon, withAlpha(rgb(inkColor), opacity));
    text(activitySwap_.title, pillTitleFont_.get(), box(pill.left + 34, pill.top, std::max(0.0f, width - 44), height),
         withAlpha(rgb(inkColor), opacity));
}

void Island::drawIsland(Clock::time_point now, float value, float amount, bool morphTitle) {
    const auto full = panelRect(now);
    const bool pendingHandoff = !pillOccluded_ && !open_ && value > 0 && activitySwapActive(now) && activitySwap_.retireOutgoing;
    const auto from = pendingHandoff ? activitySwap_.pill : compactShape(now, amount);
    morphTitle = morphTitle && !pendingHandoff;
    const float fromRadius = std::min(from.right - from.left, from.bottom - from.top) / 2;
    auto clip = mixRect(from, full, value);
    const float breadth = value + 0.18f * value * (1 - value);
    clip.left = std::lerp(from.left, full.left, breadth);
    clip.right = std::lerp(from.right, full.right, breadth);
    if (open_) panelWheel_ = clip;
    else closingPanel_ = clip;
    const float radius = std::lerp(fromRadius, panelRadius, value);

    if (!open_) {
        // Layered windows receive pointer input only where pixels have nonzero alpha.
        const auto hit = compactHit(from, amount, pillOccluded_);
        target_->FillRectangle(hit, ink(D2D1::ColorF(0, 0, 0, 1.0f / 255)));
        addTarget(hit, Hit::pill);
    }
    fillShell(rounded(clip, radius));
    target_->DrawRoundedRectangle(rounded(grow(clip, -0.5f, -0.5f), radius - 0.5f),
                                  ink(white(std::lerp(0.13f + 0.23f * dotHover(now) * (1 - clamp01(amount)), 0.12f, value))), 1);
    const float compactOpacity = lin(0.55f, 1, amount) * (1 - lin(0, 0.38f, value));
    const float panelOpacity = lin(0.16f, 0.86f, value);
    const float switching = progress(screenStart_, screenFadeTime, now);
    const bool sharedArt = !pendingHandoff && screen_ == Screen::media && media_.hasSession && pillMode_ == PillMode::media &&
                           !pillLyric_.layout && !previousPillLyric_.layout &&
                           (switching >= 1 || screenFrom_ == screen_) && amount >= 1 && value > 0 && value < 1;
    if (compactOpacity <= 0 && panelOpacity <= 0 && !morphTitle && !sharedArt) return;

    winrt::com_ptr<ID2D1RoundedRectangleGeometry> mask;
    winrt::check_hresult(graphics_->CreateRoundedRectangleGeometry(rounded(clip, radius), mask.put()));
    pushLayer(mask.get(), 1);
    if (compactOpacity > 0) {
        if (compactOpacity < 1) pushLayer(nullptr, compactOpacity);
        if (pendingHandoff) drawHandoffOutgoing(from, 1);
        else drawCompactContent(now, from, morphTitle, sharedArt);
        if (compactOpacity < 1) popLayer();
    }

    recordPanel_ = open_ && value >= 1;
    auto content = full;
    const float inset = inlineInset();
    content.top += navHeightAt(now);
    if (panelOpacity > 0) {
        if (panelOpacity < 1) pushLayer(nullptr, panelOpacity);
        if (inset > 0) drawInlineAlert(content);
        content.top += inset;
        const float artOpacity = sharedArt ? 0 : lin(0.55f, 0.9f, value);
        if (switching < 1 && screenFrom_ != screen_) {
            const float enter = easeInOut(switching);
            pushLayer(nullptr, 1 - enter);
            drawScreen(screenFrom_, content, now, artOpacity, false, false);
            popLayer();
            if (enter > 0) {
                pushLayer(nullptr, enter);
                drawScreen(screen_, content, now, artOpacity, false, recordPanel_);
                popLayer();
            }
        } else {
            drawScreen(screen_, content, now, artOpacity, morphTitle, recordPanel_);
        }
        drawNav(full, now, recordPanel_);
        if (recordPanel_ && sourcePickerOpen_) drawSourceMenu(content);
        if (recordPanel_ && prayerPickerOpen_ && screen_ == Screen::prayer) drawPrayerLocationMenu(content);
        if (recordPanel_ && audioPickerOpen_ && screen_ == Screen::system) drawAudioOutputMenu(content);
        if (panelOpacity < 1) popLayer();
    }
    if (sharedArt) {
        const auto panelArt = box(content.left + padX, content.top + mediaCoverTop, mediaCoverSize, mediaCoverSize);
        drawArt(mixRect(pillArtRect(from), panelArt, value), 1);
    }
    if (morphTitle) drawSharedTitle(from, content, value);
    popLayer();
}

void Island::drawScreen(Screen screen, D2D1_RECT_F panel, Clock::time_point now, float artOpacity,
                        bool hideTitle, bool record) {
    if (!preferences_.enabled(screen)) {
        if (screens_.empty()) text(L"No features enabled", bodyFont_.get(),
            box(panel.left + padX, panel.top + padTop, panel.right - panel.left - 2 * padX, 24), rgb(mutedColor));
        return;
    }
    switch (screen) {
    case Screen::media: drawMedia(panel, now, artOpacity, hideTitle, record); break;
    case Screen::timers: drawTimers(panel, now, record); break;
    case Screen::scratch: drawScratchpad(panel, record); break;
    case Screen::drop: drawDropZone(panel, record); break;
    case Screen::system: drawSystem(panel, record); break;
    case Screen::prayer: drawPrayerSchedule(panel, record); break;
    }
}

void Island::drawMedia(D2D1_RECT_F panel, Clock::time_point now, float artOpacity, bool hideTitle, bool record) {
    const float x = panel.left;
    const float y = panel.top;
    const bool hasMedia = media_.hasSession;
    constexpr float mediaInnerWidth = mediaPanelWidth - 2 * padX;
    const auto cover = box(x + padX, y + mediaCoverTop, mediaCoverSize, mediaCoverSize);
    if (hasMedia) drawArt(cover, artOpacity);
    else {
        target_->FillRoundedRectangle(rounded(cover, 8), ink(white(0.07f)));
        drawScreenGlyph(Screen::media, cover, rgb(mutedColor));
    }
    if (hasMedia && !media_.appId.empty()) {
        if (hovered(Hit::cover) || pressed(Hit::cover))
            target_->DrawRoundedRectangle(rounded(cover, 8), ink(accent_), 1);
        if (record) addTarget(cover, Hit::cover);
    }
    if (!hideTitle)
        text(hasMedia ? title() : L"Not playing", mediaTitleFont_.get(), box(x + mediaTitleLeft, y + mediaTitleTop, mediaTitleWidth, mediaTitleHeight),
             rgb(inkColor));
    const auto& detail = media_.artist.empty() ? media_.album : media_.artist;
    const auto artist = box(x + mediaTitleLeft, y + mediaTitleTop + mediaTitleHeight, mediaTitleWidth, mediaArtistHeight);
    text(detail, bodyFont_.get(), artist, rgb(mutedColor));
    if (hasMedia && preferences_.showWaveform) {
        const float pauseSeconds = std::chrono::duration<float>(now - pausedAt_).count();
        const float strength = !animate_ ? 0 : media_.playing ? 1 : 1 - lin(0, 0.35f, pauseSeconds);
        constexpr float barWidth = 4;
        const float top = artist.bottom + 5;
        const float bottom = cover.bottom - 2;
        const float step = (mediaTitleWidth - barWidth) / static_cast<float>(spectrum_.size() - 1);
        for (size_t i = 0; i < spectrum_.size(); ++i) {
            const float level = spectrum_[i] * strength;
            const float height = std::lerp(2.0f, bottom - top, level);
            const float left = x + mediaTitleLeft + static_cast<float>(i) * step;
            target_->FillRoundedRectangle(rounded(box(left, bottom - height, barWidth, height), 1.5f),
                                          ink(withAlpha(accent_, std::lerp(0.35f, 0.92f, level))));
        }
    }

    if (hasMedia && !media_.sources.empty()) {
        const auto selected = std::ranges::find_if(media_.sources, [this](MediaSource const& source) {
            return source.id == media_.sourceId && source.appId == media_.appId;
        });
        const auto& source = selected != media_.sources.end() ? *selected : media_.sources.front();
        const auto picker = box(x + padX, y + mediaActionsTop, 38, mediaActionHeight);
        const float middle = picker.top + mediaActionHeight / 2;
        const bool multiple = media_.sources.size() > 1;
        if (multiple) {
            drawButton(picker, L"", Hit::sourcePicker, record, 0, sourcePickerOpen_);
            const auto color = ink(rgb(mutedColor));
            const float direction = sourcePickerOpen_ ? -1.0f : 1.0f;
            target_->DrawLine(pt(picker.right - 12, middle - 2 * direction), pt(picker.right - 8, middle + 2 * direction), color, 1);
            target_->DrawLine(pt(picker.right - 8, middle + 2 * direction), pt(picker.right - 4, middle - 2 * direction), color, 1);
        }
        drawSourceIcon(source, pt(picker.left + (multiple ? 11 : 19), middle));
    }

    constexpr float lyricButtonWidth = 34;
    const auto lyricButton = box(x + mediaPanelWidth - padX - lyricButtonWidth, y + mediaActionsTop,
                                 lyricButtonWidth, mediaActionHeight);
    if (hasMedia) {
        drawButton(lyricButton, L"", Hit::lyrics, record, 0, lyricsOpen_);
        drawLyricsGlyph(pt(lyricButton.left + 9, lyricButton.top + 8), lyricsOpen_ ? rgb(readoutColor) : rgb(inkColor));
    }

    // Timeline.
    const auto rail = box(x + padX, cover.bottom + 9, mediaInnerWidth, 15);
    timelineRect_ = rail;
    const float middle = rail.top + 7.5f;
    const bool timed = hasMedia && media_.duration > 0;
    const double position = media_.positionAt(now);
    const float fraction = dragging_ ? dragFraction_ : timed ? static_cast<float>(position / media_.duration) : 0;
    target_->FillRoundedRectangle(rounded(box(rail.left, middle - 2, mediaInnerWidth, 4), 2), ink(white(0.18f)));
    if (timed) {
        const float end = rail.left + mediaInnerWidth * fraction;
        target_->FillRoundedRectangle(rounded(D2D1::RectF(rail.left, middle - 2, end, middle + 2), 2), ink(accent_));
        if (media_.canSeek) {
            const float knob = dragging_ || hovered(Hit::timeline) ? 7.0f : 6.0f;
            target_->FillEllipse(D2D1::Ellipse({end, middle}, knob, knob), ink(accent_));
            if (record) addTarget(grow(rail, 0, 4), Hit::timeline);
        }
        const double shown = dragging_ ? fraction * media_.duration : position;
        const bool showHours = media_.duration >= 3600;
        text(timestamp(shown, showHours), monoFont_.get(), box(rail.left, rail.bottom, mediaInnerWidth / 2, 14), rgb(mutedColor));
        text(timestamp(media_.duration, showHours), monoEndFont_.get(),
             box(rail.left + mediaInnerWidth / 2, rail.bottom, mediaInnerWidth / 2, 14), rgb(mutedColor));
    } else if (hasMedia) {
        text(L"Live", monoFont_.get(), box(rail.left, rail.bottom, mediaInnerWidth / 2, 14), rgb(mutedColor));
    }

    // Transport: previous, play/pause on an accent disc, next.
    const float middleY = y + mediaControlsCenterY;
    constexpr float transportRadius = 22;
    const float centerX = x + mediaPanelWidth / 2;
    const Hit buttons[]{Hit::previous, Hit::toggle, Hit::next};
    const auto onAccent = mixColor(accent_, D2D1::ColorF(0, 0, 0), 0.86f);
    for (int i = 0; i < 3; ++i) {
        const bool enabled = hasMedia && (buttons[i] == Hit::toggle || media_.canSkip);
        const D2D1_POINT_2F c{centerX + (i - 1) * 52.0f, middleY};
        const auto disc = D2D1::Ellipse(c, transportRadius, transportRadius);
        if (buttons[i] == Hit::toggle) {
            auto face = hasMedia ? accent_ : white(0.10f);
            if (pressed(Hit::toggle)) face = mixColor(face, D2D1::ColorF(0, 0, 0), 0.18f);
            else if (hovered(Hit::toggle)) face = mixColor(face, D2D1::ColorF(1, 1, 1), 0.12f);
            target_->FillEllipse(disc, ink(face));
            if (media_.playing) {
                target_->FillRectangle(box(c.x - 5, c.y - 6, 3.5f, 12), ink(onAccent));
                target_->FillRectangle(box(c.x + 1.5f, c.y - 6, 3.5f, 12), ink(onAccent));
            } else {
                fillPolygon({{c.x - 4, c.y - 6}, {c.x + 6, c.y}, {c.x - 4, c.y + 6}}, hasMedia ? onAccent : rgb(mutedColor));
            }
        } else {
            if (enabled && pressed(buttons[i])) target_->FillEllipse(disc, ink(white(0.14f)));
            else if (enabled && hovered(buttons[i])) target_->FillEllipse(disc, ink(white(0.08f)));
            const auto color = enabled ? rgb(inkColor) : rgb(mutedColor, 0.42f);
            const float d = buttons[i] == Hit::next ? 1.0f : -1.0f;
            fillPolygon({{c.x - 6 * d, c.y - 5.5f}, {c.x + 5 * d, c.y}, {c.x - 6 * d, c.y + 5.5f}}, color);
            const float bar = d > 0 ? c.x + 4 : c.x - 6;
            target_->FillRectangle(box(bar, c.y - 5.5f, 2, 11), ink(color));
        }
        if (record && enabled)
            addTarget(box(c.x - transportRadius, c.y - transportRadius, 2 * transportRadius, 2 * transportRadius),
                      buttons[i]);
    }

    if (panel.right - panel.left > mediaPanelWidth) {
        drawLyrics(panel, now, dragging_ ? dragFraction_ * media_.duration : position);
    }
}

void Island::drawSourceIcon(MediaSource const& source, D2D1_POINT_2F c) {
    if (auto* logo = icon(source.appId); logo && bitmap(*logo)) {
        target_->DrawBitmap(logo->bitmap.get(), box(c.x - 9, c.y - 9, 18, 18), 1, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    } else {
        target_->FillEllipse(D2D1::Ellipse(c, 9, 9), ink(white(0.16f)));
        const auto name = appName(source.appId);
        text(name.substr(0, 1), letterFont_.get(), box(c.x - 9, c.y - 9, 18, 18), rgb(inkColor));
    }
    if (source.tabNumber > 0) {
        const auto badge = pt(c.x + 8, c.y + 8);
        target_->FillEllipse(D2D1::Ellipse(badge, 6, 6), ink(rgb(0x253347)));
        text(std::to_wstring(source.tabNumber), letterFont_.get(), box(badge.x - 6, badge.y - 6, 12, 12), rgb(inkColor));
    }
}

void Island::drawSourceMenu(D2D1_RECT_F panel) {
    const size_t total = media_.sources.size();
    const size_t rows = (total + sourceColumns - 1) / sourceColumns;
    sourceOffset_ = std::min(sourceOffset_, rows > 2 ? (rows - 2) * sourceColumns : 0);
    const size_t count = std::min(visibleSources, total - sourceOffset_);
    const float width = 2 * pickerInset + sourceMenuStep * static_cast<float>(std::min(count, sourceColumns));
    sourceMenuRect_ = box(panel.left + padX, panel.top + sourceMenuTop, width, sourcePickerHeight(count));
    drawPickerSurface(sourceMenuRect_, Hit::sourceMenu);
    for (size_t slot = 0; slot < count; ++slot) {
        const size_t i = sourceOffset_ + slot;
        const auto& source = media_.sources[i];
        const auto rect = box(sourceMenuRect_.left + pickerInset + static_cast<float>(slot % sourceColumns) * sourceMenuStep,
                              sourceMenuRect_.top + pickerInset + static_cast<float>(slot / sourceColumns) * sourceMenuStep, 33, 33);
        const D2D1_POINT_2F c{rect.left + 16, rect.top + 16};
        const bool selected = source.id.empty() ? media_.sourceId.empty() && source.appId == media_.appId :
                                                 source.id == media_.sourceId;
        drawPickerRow(rect, L"", Hit::source, i, selected);
        drawSourceIcon(source, selected ? pt(c.x - 4, c.y) : c);
    }
}

void Island::prepareLyricLayout(float width) {
    if (lyricLayout_ && lyricLayoutRevision_ == lyrics_->revision && lyricLayoutWidth_ == width) return;
    if (pressed_ == Hit::lyricLine) pressed_ = Hit::none;
    const bool newLyrics = lyricLayoutRevision_ != lyrics_->revision;

    std::wstring joined = lyrics_->plain;
    lyricRanges_.clear();
    for (const auto& line : lyrics_->lines) {
        if (!lyricRanges_.empty()) joined += L'\n';
        lyricRanges_.push_back({static_cast<UINT32>(joined.size()), static_cast<UINT32>(line.text.size())});
        joined += line.text;
    }

    winrt::com_ptr<IDWriteTextLayout> layout;
    winrt::check_hresult(fonts_->CreateTextLayout(joined.data(), static_cast<UINT32>(joined.size()),
        lyricFont_.get(), width, 1'000'000, layout.put()));
    DWRITE_TEXT_METRICS metrics{};
    winrt::check_hresult(layout->GetMetrics(&metrics));
    lyricLayout_ = std::move(layout);
    lyricContentHeight_ = metrics.height;
    lyricLayoutWidth_ = width;
    lyricLayoutRevision_ = lyrics_->revision;
    activeLyric_ = -1;
    activeLyricTop_ = activeLyricHeight_ = 0;
    if (newLyrics) {
        lyricScrollFrom_ = lyricScrollTo_ = lyricScrollShown_ = 0;
        lyricScrollStart_ = lyricFollowAt_ = {};
        lyricFollowLeft_ = {};
    }
    if (!lyricActiveInk_) {
        winrt::check_hresult(target_->CreateSolidColorBrush(rgb(inkColor), lyricActiveInk_.put()));
    }
}

void Island::drawLyrics(D2D1_RECT_F panel, Clock::time_point now, double position) {
    if (!lyrics_) return;
    const float x = panel.left + mediaPanelWidth + 8, y = panel.top + 16;
    const float width = lyricsWidth_ - padX - 8;
    const float viewportBottom = panel.bottom - y - padBottom - 6;
    text(L"Lyrics", titleFont_.get(), box(x, y, width, 22), rgb(inkColor));
    const bool synced = lyrics_->status == LyricsStatus::ready;
    const bool plain = lyrics_->status == LyricsStatus::plain;
    if (!plain && (!synced || media_.duration <= 0)) {
        auto status = L"Finding lyrics…";
        if (media_.duration <= 0) status = L"Playback timing unavailable";
        else if (lyrics_->status == LyricsStatus::unavailable) status = L"No lyrics found";
        else if (lyrics_->status == LyricsStatus::error) status = L"Couldn’t load lyrics";
        text(status, bodyFont_.get(), box(x, y + 34, width, 20), rgb(mutedColor));
        if (lyrics_->status == LyricsStatus::error && media_.duration > 0) {
            drawButton(box(x, y + 61, 64, 27), L"Retry", Hit::retryLyrics, recordPanel_ && wantsLyrics());
        }
        return;
    }

    prepareLyricLayout(width);
    if (plain && width >= 130) {
        text(L"Unsynced", bodyEndFont_.get(), box(x + 57, y, width - 57, 22), rgb(mutedColor));
    }
    const float viewportTop = synced ? 50.0f : 34.0f;
    const float viewportHeight = std::max(0.0f, viewportBottom - viewportTop);
    if (synced) {
        constexpr float controlWidth = 100;
        constexpr float buttonWidth = 24;
        const float left = x + width - controlWidth;
        text(L"Delay", buttonFont_.get(), box(left, y, controlWidth, 22), rgb(mutedColor));
        if (media_.canSeek && width >= 220) {
            text(L"Click a line to seek", bodyFont_.get(), box(x, y + 21, width - controlWidth - 8, 24),
                 rgb(mutedColor));
        }
        wchar_t delay[16]{};
        const double seconds = lyrics_->delaySeconds();
        swprintf_s(delay, seconds == 0 ? L"%.1f s" : L"%+.1f s", seconds);
        const bool record = recordPanel_ && wantsLyrics();
        drawButton(box(left, y + 21, buttonWidth, 24), L"−", Hit::lyricsDelay, record, 0);
        text(delay, buttonFont_.get(), box(left + buttonWidth, y + 21, controlWidth - 2 * buttonWidth, 24), rgb(inkColor));
        drawButton(box(left + controlWidth - buttonWidth, y + 21, buttonWidth, 24),
                   L"+", Hit::lyricsDelay, record, 1);
    }
    const auto& lines = lyrics_->lines;
    const auto next = std::ranges::upper_bound(lines, position - lyrics_->delaySeconds(), {}, &LyricLine::seconds);
    const auto active = (next - lines.begin()) - 1;
    if (active != activeLyric_) {
        if (activeLyric_ >= 0 && lyricRanges_[activeLyric_].length != 0) {
            winrt::check_hresult(lyricLayout_->SetDrawingEffect(nullptr, lyricRanges_[activeLyric_]));
        }
        activeLyric_ = active;
        activeLyricTop_ = activeLyricHeight_ = 0;
        if (active >= 0) {
            const auto range = lyricRanges_[active];
            if (range.length != 0) {
                winrt::check_hresult(lyricLayout_->SetDrawingEffect(lyricActiveInk_.get(), range));
            }
            DWRITE_HIT_TEST_METRICS hit{};
            float left = 0;
            winrt::check_hresult(lyricLayout_->HitTestTextPosition(range.startPosition, FALSE,
                &left, &activeLyricTop_, &hit));
            float bottom = lyricContentHeight_;
            if (next != lines.end()) {
                winrt::check_hresult(lyricLayout_->HitTestTextPosition(lyricRanges_[active + 1].startPosition,
                    FALSE, &left, &bottom, &hit));
            }
            activeLyricHeight_ = bottom - activeLyricTop_;
        }
    }

    if (synced && wantsLyrics() && now >= lyricFollowAt_ && pressed_ != Hit::lyricLine) {
        const float lead = std::max(0.0f, (viewportHeight - activeLyricHeight_) * 0.36f);
        const float limit = std::max(0.0f, lyricContentHeight_ - viewportHeight);
        moveLyricScroll(std::clamp(activeLyricTop_ - lead, 0.0f, limit), now);
    }
    lyricViewport_ = box(x, y + viewportTop, width, viewportHeight);
    lyricScrollShown_ = std::clamp(lyricScroll(now), 0.0f, std::max(0.0f, lyricContentHeight_ - viewportHeight));
    target_->PushAxisAlignedClip(lyricViewport_, D2D1_ANTIALIAS_MODE_ALIASED);
    target_->DrawTextLayout(pt(x, lyricViewport_.top - lyricScrollShown_), lyricLayout_.get(),
                            ink(rgb(plain ? inkColor : mutedColor)));
    target_->PopAxisAlignedClip();
}

void Island::drawTimers(D2D1_RECT_F panel, Clock::time_point now, bool record) {
    const float x = panel.left;
    const float y = panel.top;
    const float centerX = x + panelWidth / 2;
    text(L"Timers", titleFont_.get(), box(x + padX, y + padTop, innerWidth, 22.4f), rgb(inkColor));
    constexpr wchar_t const* labels[]{L"Focus", L"Break", L"Quick Timer"};
    const size_t phase = breakPhase_ ? 1 : 0;
    const size_t selected = quickView_ ? 2 : phase;
    const bool focusBusy = focus_.running() || focus_.paused();

    // Auto toggle at the top right; with it on, four dots show the round (filled = Focus finished).
    const auto autoButton = box(x + panelWidth - padX - 46, y + 11, 46, 25);
    drawButton(autoButton, L"Auto", Hit::autoCycle, record, 0, autoCycle_);
    if (autoCycle_) {
        for (int i = 0; i < focusRounds; ++i) {
            const auto center = pt(autoButton.left - 12 - 12.0f * static_cast<float>(focusRounds - 1 - i),
                                   (autoButton.top + autoButton.bottom) / 2);
            const bool finished = i < focusRound_ - (breakPhase_ ? 0 : 1);
            const bool current = i == focusRound_ - 1 && focusBusy && !finished;
            if (finished) target_->FillEllipse(D2D1::Ellipse(center, 3.5f, 3.5f), ink(accent_));
            else target_->DrawEllipse(D2D1::Ellipse(center, 3, 3), ink(current ? accent_ : white(0.35f)), 1.2f);
        }
    }
    for (size_t i = 0; i < std::size(labels); ++i) {
        const bool selectable = i == 2 || !focusBusy || i == phase;
        drawButton(box(centerX - 116 + 80 * static_cast<float>(i), y + 43, 72, 28),
                   labels[i], Hit::timerView, record && selectable, i, i == selected);
    }

    const auto& timer = quickView_ ? quick_ : focus_;
    const bool adjustable = !timer.running() && !timer.paused();
    const auto clock = box(centerX - 79, y + 77, 158, 44);
    const int seconds = timer.seconds(now);
    const float roll = adjustable && animate_ && timerDigitsTo_ == seconds
        ? easeInOut(progress(timerDigitsStart_, timerDigitMotion, now)) : 1;
    const float direction = timerDigitsTo_ > timerDigitsFrom_ ? 1.0f : -1.0f;
    text(L":", clockFont_.get(), box(centerX - 6, clock.top, 12, 44), rgb(mutedColor));
    for (size_t unit = 0; unit < 2; ++unit) {
        const auto area = box(unit == 0 ? centerX - 78 : centerX + 8, clock.top, 70, 44);
        const auto digits = [unit](int time) {
            const int value = unit == 0 ? time / 60 : time % 60;
            return (value < 10 ? L"0" : L"") + std::to_wstring(value);
        };
        if (adjustable && (hovered(Hit::timerDuration, unit) || pressed(Hit::timerDuration, unit)))
            target_->FillRoundedRectangle(rounded(area, 8), ink(white(0.08f)));
        if (roll < 1 && digits(timerDigitsFrom_) != digits(seconds)) {
            target_->PushAxisAlignedClip(area, D2D1_ANTIALIAS_MODE_ALIASED);
            text(digits(timerDigitsFrom_), clockFont_.get(),
                 box(area.left, area.top - direction * 34 * roll, 70, 44), white(1 - roll));
            text(digits(seconds), clockFont_.get(),
                 box(area.left, area.top + direction * 34 * (1 - roll), 70, 44), white(roll));
            target_->PopAxisAlignedClip();
        } else {
            text(digits(seconds), clockFont_.get(), area, rgb(inkColor));
        }
        if (record && adjustable) addTarget(area, Hit::timerDuration, unit);
    }
    if (record && adjustable) timerEditor_ = clock;
    if (quickView_) {
        for (size_t i = 0; i < std::size(timerPresets); ++i) {
            const auto minutes = timerPresets[i];
            drawButton(box(centerX - 88 + 60 * static_cast<float>(i), y + 126, 56, 26),
                       std::to_wstring(minutes) + L" min", Hit::timerPreset, record && adjustable, i,
                       quick_.duration() == minutes * 60);
        }
    } else {
        const auto detail = autoCycle_
            ? phaseName(breakPhase_) + L" · round " + std::to_wstring(focusRound_) + L" of " + std::to_wstring(focusRounds)
            : timestamp(focus_.duration()) + L" set";
        text(detail, buttonFont_.get(), box(x + padX, y + 126, innerWidth, 26), rgb(mutedColor));
    }
    if (adjustable) {
        const auto hit = quickView_ ? Hit::quickAdjust : Hit::focusAdjust;
        const int duration = timer.duration();
        const int limit = timerDurationLimit();
        for (size_t unit = 0; unit < 2; ++unit) {
            const float left = centerX - 111 + 117 * static_cast<float>(unit);
            drawButton(box(left, y + 157, 28, 26), L"−", hit, record && duration > 1, 2 * unit);
            text(unit == 0 ? L"min" : L"sec", buttonFont_.get(),
                 box(left + 30, y + 157, 45, 26), rgb(mutedColor));
            drawButton(box(left + 77, y + 157, 28, 26), L"+", hit, record && duration < limit, 2 * unit + 1);
        }
    }
    auto action = L"Start";
    if (timer.running()) action = L"Pause";
    else if (timer.paused()) action = L"Resume";
    else if (timer.done()) action = quickView_ ? L"Restart" : (breakPhase_ ? L"Start Focus" : L"Start Break");
    drawButton(box(centerX - 93, y + 191, 106, 29), action,
               quickView_ ? Hit::timerToggle : Hit::focusToggle, record);
    drawButton(box(centerX + 21, y + 191, 72, 29), L"Reset",
               quickView_ ? Hit::timerReset : Hit::focusReset, record);
}

void Island::drawScratchpad(D2D1_RECT_F panel, bool record) {
    const float x = panel.left + padX;
    const float y = panel.top + padTop;
    text(L"Scratchpad", titleFont_.get(), box(x, y, innerWidth, 22), rgb(inkColor));
    if (scratchpad_ && scratchpad_->saveFailed) {
        text(L"Couldn’t save", bodyEndFont_.get(), box(x + 160, y, innerWidth - 160, 22), rgb(mutedColor));
    }
    const auto field = box(x, y + 32, innerWidth, 116);
    const auto edge = scratchEditing_ || hovered(Hit::scratchEdit) ? accent_ : rgb(0x737e8c);
    target_->FillRoundedRectangle(rounded(field, 6), ink(rgb(0x161c24)));
    target_->DrawRoundedRectangle(rounded(field, 6), ink(edge), 1);
    const auto editor = grow(field, -3, -3);
    if (!scratchEditing_) {
        const auto viewport = grow(editor, -7, -4);
        if (!scratchpad_ || scratchpad_->text.empty()) {
            scratchLayout_ = nullptr;
            scratchLayoutText_.clear();
            scratchScroll_ = scratchScrollLimit_ = 0;
            text(L"Write a quick note…", scratchFont_.get(), viewport, rgb(mutedColor));
        } else {
            const float width = viewport.right - viewport.left;
            if (!scratchLayout_ || scratchLayoutText_ != scratchpad_->text || std::abs(scratchLayout_->GetMaxWidth() - width) > .1f) {
                scratchLayoutText_ = scratchpad_->text;
                scratchLayout_ = nullptr;
                winrt::check_hresult(fonts_->CreateTextLayout(scratchLayoutText_.data(),
                    static_cast<UINT32>(scratchLayoutText_.size()), scratchFont_.get(), width, 1'000'000,
                    scratchLayout_.put()));
                DWRITE_TEXT_METRICS metrics{};
                winrt::check_hresult(scratchLayout_->GetMetrics(&metrics));
                scratchScrollLimit_ = std::max(0.0f, metrics.height - (viewport.bottom - viewport.top));
                scratchScroll_ = std::min(scratchScroll_, scratchScrollLimit_);
            }
            target_->PushAxisAlignedClip(viewport, D2D1_ANTIALIAS_MODE_ALIASED);
            target_->DrawTextLayout(pt(viewport.left, viewport.top - scratchScroll_), scratchLayout_.get(),
                                    ink(rgb(inkColor)), D2D1_DRAW_TEXT_OPTIONS_CLIP);
            target_->PopAxisAlignedClip();
        }
    }
    if (record) {
        scratchEditor_ = editor;
        addTarget(field, Hit::scratchEdit);
    }
}

void Island::drawDropZone(D2D1_RECT_F panel, bool record) {
    const float x = panel.left + padX;
    const float y = panel.top + padTop;
    text(L"Drop Zone", titleFont_.get(), box(x, y, innerWidth, 22), rgb(inkColor));
    const bool empty = dropItems_.empty();
    const auto field = box(x, panel.top + 44, innerWidth, empty ? 112.0f : 36.0f);
    const auto blue = rgb(0x9ccfff);
    target_->FillRoundedRectangle(rounded(field, 9), ink(withAlpha(blue, dropHover_ ? 0.11f : 0.03f)));
    target_->DrawRoundedRectangle(rounded(field, 9), ink(dropHover_ ? blue : rgb(0x657b94)), 1, dropDash_.get());
    if (empty) {
        drawScreenGlyph(Screen::drop, box(x, field.top + 22, innerWidth, 24), blue);
        text(dropHover_ ? L"Release to add" : L"Drop files or text here", headingCenterFont_.get(),
             box(x, field.top + 53, innerWidth, 26), rgb(inkColor));
    } else {
        drawScreenGlyph(Screen::drop, box(x + innerWidth / 2 - 58, field.top, 22, 36), blue);
        text(dropHover_ ? L"Release to add" : L"Drop to add", bodyFont_.get(),
             box(x + innerWidth / 2 - 29, field.top, 115, 36), rgb(inkColor));
    }

    if (!empty) drawButton(box(x + innerWidth - 70, y, 70, 25), L"Clear all", Hit::clearDrop, record);
    const auto end = std::min(dropItems_.size(), dropOffset_ + visibleDropRows);
    const float rowsTop = field.bottom + 6;
    const auto rows = box(x, rowsTop, innerWidth, dropRowHeight * static_cast<float>(end - dropOffset_));
    if (record) dropViewport_ = rows;
    for (size_t index = dropOffset_; index < end; ++index) {
        const float rowY = rowsTop + dropRowHeight * static_cast<float>(index - dropOffset_);
        const auto drag = box(x, rowY + 1, innerWidth - 36, dropRowHeight - 2);
        const auto remove = box(x + innerWidth - 30, rowY + 7, 30, 29);
        const auto tile = box(x + 4, rowY + 5, 44, 33);
        if (index > dropOffset_) target_->FillRectangle(box(x, rowY, innerWidth, 1), ink(white(0.06f)));
        if (hovered(Hit::dropItem, index)) target_->FillRoundedRectangle(rounded(drag, 6), ink(white(0.06f)));
        target_->FillRoundedRectangle(rounded(tile, 5), ink(rgb(0x1c2635)));
        const auto& item = dropItems_[index];
        if (record) addTarget(drag, Hit::dropItem, index);
        if (item.thumbnail) {
            const auto& image = *item.thumbnail;
            auto& bitmap = dropBitmaps_[&image];
            if (!bitmap) {
                const auto properties = D2D1::BitmapProperties(
                    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
                target_->CreateBitmap(D2D1::SizeU(image.width, image.height), image.pixels.data(),
                                      image.width * 4, properties, bitmap.put());
            }
            if (bitmap) {
                const float scale = std::min(42.0f / image.width, 31.0f / image.height);
                const float width = image.width * scale, height = image.height * scale;
                target_->DrawBitmap(bitmap.get(), box(tile.left + (44 - width) / 2, tile.top + (33 - height) / 2,
                    width, height), 1, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            }
            const bool active = hovered(Hit::dropImage, index) || pressed(Hit::dropImage, index);
            target_->DrawRoundedRectangle(rounded(tile, 5), ink(active ? blue : white(0.16f)), 1);
            if (record) addTarget(tile, Hit::dropImage, index);
        } else {
            const auto color = ink(rgb(0xa8c4e2));
            const float left = tile.left + 16, top = tile.top + 9;
            if (!item.text) target_->DrawRoundedRectangle(rounded(box(left, top, 12, 15), 2), color, 1);
            const float inset = item.text ? 0.0f : 3.0f;
            for (int line = 0; line < 3; ++line) {
                const float lineY = top + 4 + line * 3.0f;
                target_->DrawLine(pt(left + inset, lineY), pt(left + 12 - inset - (line == 2 ? 2 : 0), lineY), color, 1);
            }
        }
        text(item.label, bodyFont_.get(), box(x + 57, rowY + 3, innerWidth - 100, 20), rgb(inkColor));
        text(item.thumbnail ? L"Image" : item.text ? L"Plain text" : L"File", bodyFont_.get(),
             box(x + 57, rowY + 22, innerWidth - 100, 15), rgb(mutedColor));
        drawButton(remove, L"×", Hit::removeDrop, record, index);
    }
    const auto count = std::to_wstring(dropItems_.size()) + (dropItems_.size() == 1 ? L" item" : L" items");
    const auto status = dropItems_.size() > visibleDropRows
        ? std::to_wstring(dropOffset_ + 1) + L"–" + std::to_wstring(end) + L" of " + count
        : count;
    const float footerY = (empty ? field.bottom : rows.bottom) + 8;
    text(empty ? L"Temporary items" : L"Drag an item to another app", bodyFont_.get(),
         box(x, footerY, 220, 18), rgb(mutedColor));
    text(status, bodyEndFont_.get(), box(x + 221, footerY, innerWidth - 221, 18), rgb(mutedColor));
}

void Island::drawSystem(D2D1_RECT_F panel, bool record) {
    const float x = (panel.left + panel.right - systemPanelWidth) / 2 + padX;
    const float y = panel.top;
    constexpr float width = systemColumnWidth;
    constexpr float fullWidth = width * 2 + systemRightGap;
    text(L"System", titleFont_.get(), box(x, y + 3, fullWidth, 23), rgb(inkColor));

    constexpr float gap = 6;
    const auto drawUsage = [&](D2D1_RECT_F bar, std::optional<unsigned> usage) {
        target_->FillRoundedRectangle(rounded(bar, 2), ink(white(0.12f)));
        if (usage && *usage) {
            auto fill = bar;
            fill.right = std::lerp(bar.left, bar.right, *usage / 100.0f);
            target_->FillRoundedRectangle(rounded(fill, 2), ink(accent_));
        }
    };
    constexpr float metricWidth = (fullWidth - 2 * gap) / 3;
    const auto metric = [&](size_t index, std::wstring_view label, std::optional<unsigned> usage) {
        const float left = x + static_cast<float>(index) * (metricWidth + gap);
        const float top = y + 30;
        target_->FillRoundedRectangle(rounded(box(left, top, metricWidth, 37), 7), ink(white(0.055f)));
        text(label, bodyFont_.get(), box(left + 6, top + 3, metricWidth - 30, 17), rgb(inkColor));
        text(usage ? std::to_wstring(*usage) + L"%" : L"—", bodyEndFont_.get(),
             box(left + 6, top + 3, metricWidth - 12, 17), rgb(usage ? inkColor : mutedColor));
        drawUsage(box(left + 7, top + 25, metricWidth - 14, 4), usage);
    };
    metric(0, L"CPU", system_.cpu);
    metric(1, L"RAM", system_.memory);
    metric(2, L"GPU", system_.gpu);

    if (!system_.disks.empty())
        text(L"Storage", bodyFont_.get(), box(x, y + systemStatsBottom, fullWidth, 18), rgb(inkColor));
    const auto columns = std::min(system_.disks.size(), storageColumns);
    for (size_t i = 0; i < system_.disks.size(); ++i) {
        const auto& disk = system_.disks[i];
        const float cardWidth = (fullWidth + gap) / static_cast<float>(columns) - gap;
        const float left = x + static_cast<float>(i % columns) * (cardWidth + gap);
        const float top = y + storageCardsTop + static_cast<float>(i / columns) * storageCardPitch;
        const auto usage = disk.space ? std::optional{static_cast<unsigned>(std::lround(
            100.0 * (disk.space->totalBytes - disk.space->freeBytes) / disk.space->totalBytes))} : std::nullopt;
        target_->FillRoundedRectangle(rounded(box(left, top, cardWidth, 51), 7), ink(white(0.055f)));
        text(std::wstring(L"Disk ") + disk.letter + L":", bodyFont_.get(),
             box(left + 8, top + 3, cardWidth - 64, 18), rgb(inkColor));
        if (usage) {
            text(std::to_wstring(*usage) + L"% used", bodyEndFont_.get(),
                 box(left + 8, top + 3, cardWidth - 16, 18), rgb(inkColor));
        }
        text(disk.space ? freeSpaceLabel(disk.space->freeBytes) : L"Unavailable",
             bodyFont_.get(), box(left + 8, top + 21, cardWidth - 16, 16), rgb(mutedColor));
        drawUsage(box(left + 8, top + 41, cardWidth - 16, 4), usage);
    }

    const float top = y + systemSectionTop(system_);
    const float right = x + width + systemRightGap;
    const auto battery = box(right, top, width, batteryCardHeight);
    target_->FillRoundedRectangle(rounded(battery, 8), ink(white(0.055f)));
    text(L"Battery", bodyFont_.get(), box(right + 8, top + 3, 100, 18), rgb(inkColor));
    std::wstring status = L"Power status unavailable";
    if (power_.available) {
        status = !power_.hasBattery ? L"No battery" :
                 power_.charging ? L"Charging" : power_.pluggedIn == true ? L"Plugged in" :
                 power_.pluggedIn == false ? L"On battery" : L"Status unavailable";
    }
    if (power_.remaining) {
        const auto minutes = std::chrono::ceil<std::chrono::minutes>(*power_.remaining).count();
        status += L" · ";
        if (minutes >= 60) status += std::to_wstring(minutes / 60) + L"h ";
        status += std::to_wstring(minutes % 60) + L"m left";
    }
    text(status, bodyFont_.get(), box(right + 8, top + 20, width - 81, 16), rgb(mutedColor));
    if (power_.percent || power_.charging) {
        const auto level = power_.percent ? std::to_wstring(*power_.percent) + L"%" : std::wstring{};
        const float levelWidth = level.empty() ? 0 : std::ceil(measure(level, strongEndFont_.get())) + 2;
        const float levelX = battery.right - 8 - levelWidth;
        drawBatteryCell(levelX - 26, top + 20, power_.percent.value_or(0), accent_, power_.charging);
        text(level, strongEndFont_.get(), box(levelX, top, levelWidth, batteryCardHeight), accent_);
    }

    text(L"Audio Output", bodyFont_.get(), box(x, top, width, 18), rgb(inkColor));
    const auto output = std::ranges::find(audioOutputs_, true, &AudioOutput::active);
    const auto outputName = output == audioOutputs_.end() ? std::wstring_view(audioOutput_) :
                                                           std::wstring_view(output->name);
    drawPickerButton(box(x, top + systemAudioTop, width, systemAudioHeight),
                     outputName.empty() ? L"Choose output" : outputName, Hit::audioPicker, record,
                     audioPickerOpen_, output != audioOutputs_.end() && output->headphones);

    text(L"App Volume", bodyFont_.get(), box(x, top + appRowsTop - 20, width, 18), rgb(inkColor));
    const auto count = appVolumes_.size();
    const auto end = std::min(count, appOffset_ + visibleAppRows);
    if (count > visibleAppRows) {
        text(std::to_wstring(appOffset_ + 1) + L"–" + std::to_wstring(end) + L" of " + std::to_wstring(count),
             bodyEndFont_.get(), box(x, top + appRowsTop - 20, width, 18), rgb(mutedColor));
    }
    const auto rows = box(x, top + appRowsTop, width, appRowHeight * visibleAppRows);
    if (record) appViewport_ = rows;
    if (appVolumes_.empty()) {
        text(L"No apps are playing audio", bodyFont_.get(), box(x, rows.top, width, appRowHeight), rgb(mutedColor));
    }
    for (size_t index = appOffset_; index < end; ++index) {
        drawAppVolume(index, box(x, rows.top + appRowHeight * static_cast<float>(index - appOffset_),
                                 width, appRowHeight), record);
    }

    const float divider = x + width + systemRightGap / 2;
    const auto devices = std::clamp<size_t>(bluetoothDevices_.size(), 1, visibleBluetoothRows);
    const auto appRows = std::clamp<size_t>(count, 1, visibleAppRows);
    const float height = std::max(appRowsTop + static_cast<float>(appRows) * appRowHeight,
                                  connectionTop + bluetoothRowsTop + static_cast<float>(devices) * bluetoothRowHeight + 29);
    target_->FillRectangle(box(divider, top + 1, 1, height - 2), ink(white(0.08f)));
    drawConnection(box(right, top + connectionTop, width, height - connectionTop), record);
}

void Island::drawAudioGlyph(D2D1_POINT_2F center, bool headphones, D2D1_COLOR_F color) {
    if (headphones) {
        drawAlertIcon(IslandAlert::Kind::bluetoothAudio, pt(center.x - 8, center.y - 8), color);
        return;
    }
    const float x = center.x - 8, y = center.y - 8;
    fillPolygon({pt(x + 1.5f, y + 6), pt(x + 4, y + 6), pt(x + 7.5f, y + 3), pt(x + 7.5f, y + 13),
                 pt(x + 4, y + 10), pt(x + 1.5f, y + 10)}, color);
    PathBuilder waves(graphics_.get());
    waves.move(pt(x + 10, y + 5.5f), false).arc(pt(x + 10, y + 10.5f), 3.5f, false, true);
    target_->DrawGeometry(waves.finish().get(), ink(color), 1.5f, roundStroke_.get());
}

void Island::drawPickerSurface(D2D1_RECT_F rect, Hit hit) {
    target_->FillRoundedRectangle(rounded(rect, 9), ink(mixColor(rgb(0x17191c), accent_, 0.075f)));
    target_->DrawRoundedRectangle(rounded(rect, 9), ink(white(0.15f)), 1);
    addTarget(rect, hit);
}

void Island::drawPickerCheck(D2D1_POINT_2F center) {
    PathBuilder check(graphics_.get());
    check.move(pt(center.x - 4, center.y), false).line(pt(center.x - 1, center.y + 3))
         .line(pt(center.x + 4, center.y - 3));
    target_->DrawGeometry(check.finish().get(), ink(accent_), 1.4f, roundStroke_.get());
}

void Island::drawPickerRow(D2D1_RECT_F row, std::wstring_view label, Hit hit, size_t index,
                           bool selected, std::optional<bool> headphones) {
    if (selected || hovered(hit, index) || pressed(hit, index)) {
        const auto fill = selected ? withAlpha(accent_, hovered(hit, index) ? 0.22f : 0.14f) :
                                     white(pressed(hit, index) ? 0.15f : 0.08f);
        target_->FillRoundedRectangle(rounded(row, 6), ink(fill));
    }
    const float middle = (row.top + row.bottom) / 2;
    if (headphones) drawAudioGlyph(pt(row.left + 14, middle), *headphones, selected ? accent_ : rgb(inkColor));
    if (!label.empty()) {
        text(label, bodyFont_.get(), D2D1::RectF(row.left + (headphones ? 27 : 8), row.top,
             row.right - (selected ? 24 : 7), row.bottom), rgb(inkColor));
    }
    if (selected) drawPickerCheck(pt(row.right - (label.empty() ? 7 : 13), middle));
    addTarget(row, hit, index);
}

void Island::drawPickerButton(D2D1_RECT_F rect, std::wstring_view label, Hit hit, bool record, bool expanded,
                              std::optional<bool> headphones) {
    drawButton(rect, L"", hit, record, 0, expanded);
    const auto color = expanded ? accent_ : rgb(inkColor);
    const float x = rect.right - 12;
    const float y = (rect.top + rect.bottom) / 2;
    if (headphones) drawAudioGlyph(pt(rect.left + 14, y), *headphones, color);
    text(label, bodyFont_.get(), D2D1::RectF(rect.left + (headphones ? 27 : 9), rect.top,
                                           rect.right - 21, rect.bottom), color);
    const float direction = expanded ? -1.0f : 1.0f;
    target_->DrawLine(pt(x - 3, y - 1.5f * direction), pt(x, y + 1.5f * direction), ink(color), 1.2f);
    target_->DrawLine(pt(x, y + 1.5f * direction), pt(x + 3, y - 1.5f * direction), ink(color), 1.2f);
}

void Island::drawAudioOutputMenu(D2D1_RECT_F panel) {
    const float x = (panel.left + panel.right - systemPanelWidth) / 2 + padX;
    const float y = panel.top + systemSectionTop(system_) + systemAudioMenuTop;
    const auto count = std::min(audioOutputs_.size(), visibleAudioOutputs);
    const auto rows = std::max<size_t>(1, count);
    audioOutputMenuRect_ = box(x, y, systemColumnWidth, pickerHeight(rows + 1));
    drawPickerSurface(audioOutputMenuRect_, Hit::audioOutputMenu);
    const auto row = [&](size_t index) {
        return box(x + pickerInset, y + pickerInset + pickerRowHeight * static_cast<float>(index),
                   systemColumnWidth - 2 * pickerInset, pickerRowHeight - 1);
    };
    for (size_t i = 0; i < count; ++i) {
        const size_t index = audioOutputOffset_ + i;
        const auto& output = audioOutputs_[index];
        drawPickerRow(row(i), output.name, Hit::audioOutputChoice, index, output.active, output.headphones);
    }
    if (!count) text(L"No outputs available", bodyFont_.get(), grow(row(0), -8, 0), rgb(mutedColor));
    drawPickerRow(row(rows), L"Sound settings…", Hit::soundSettings, 0, false);
}

void Island::drawConnection(D2D1_RECT_F column, bool record) {
    const float x = column.left;
    const float y = column.top;
    const float width = column.right - x;
    const auto card = box(x, y, width, 47);
    target_->FillRoundedRectangle(rounded(card, 8), ink(white(0.055f)));
    drawAlertIcon(IslandAlert::Kind::connection, pt(x + 8, y + 9), accent_);
    std::wstring_view type = L"Network";
    switch (network_.type) {
    case NetworkType::wifi: type = L"Wi-Fi"; break;
    case NetworkType::ethernet: type = L"Ethernet"; break;
    case NetworkType::cellular: type = L"Cellular"; break;
    case NetworkType::other: break;
    }
    std::wstring_view access;
    switch (network_.access) {
    case NetworkAccess::unknown: access = L"Unavailable"; break;
    case NetworkAccess::disconnected: access = L"Disconnected"; break;
    case NetworkAccess::local: access = L"No internet"; break;
    case NetworkAccess::limited: access = L"Limited internet"; break;
    case NetworkAccess::internet: access = L"Online"; break;
    }
    const auto name = network_.name.empty() ? type : std::wstring_view(network_.name);
    text(name, bodyFont_.get(), box(x + 30, y + 4, width - 69, 18), rgb(inkColor));
    text(type, bodyFont_.get(), box(x + 30, y + 24, 72, 17), rgb(mutedColor));
    text(access, bodyEndFont_.get(), box(card.right - 122, y + 24, 114, 17),
         network_.access == NetworkAccess::internet ? accent_ : rgb(mutedColor));
    if (network_.signalBars) {
        for (unsigned i = 0; i < 5; ++i) {
            const float height = 3.0f + 2.0f * i;
            target_->FillRoundedRectangle(rounded(box(card.right - 26 + 4.0f * i, y + 19 - height, 2.5f, height), 1),
                                          ink(i < *network_.signalBars ? accent_ : white(0.15f)));
        }
    }

    text(L"Bluetooth", bodyFont_.get(), box(x, y + 52, width, 20), rgb(inkColor));
    const auto count = bluetoothDevices_.size();
    const auto end = std::min(count, bluetoothOffset_ + visibleBluetoothRows);
    const auto summary = count > visibleBluetoothRows ?
        std::to_wstring(bluetoothOffset_ + 1) + L"–" + std::to_wstring(end) + L" of " + std::to_wstring(count) :
        count ? std::to_wstring(count) + L" connected" : std::wstring{};
    text(summary, bodyEndFont_.get(), box(x, y + 52, width, 20), rgb(mutedColor));
    const auto viewport = box(x, y + bluetoothRowsTop, width, bluetoothRowHeight * visibleBluetoothRows);
    if (record) bluetoothViewport_ = viewport;
    if (bluetoothDevices_.empty()) {
        text(L"No Bluetooth devices connected", bodyFont_.get(), box(x, viewport.top, width, bluetoothRowHeight), rgb(mutedColor));
    }
    const auto output = audioOutput_.find(L'(');
    const auto outputName = output != std::wstring::npos && audioOutput_.ends_with(L')')
        ? audioOutput_.substr(output + 1, audioOutput_.size() - output - 2) : audioOutput_;
    for (size_t i = bluetoothOffset_; i < end; ++i) {
        const auto& device = bluetoothDevices_[i];
        const float top = viewport.top + bluetoothRowHeight * static_cast<float>(i - bluetoothOffset_);
        const bool active = device.audio && _wcsicmp(outputName.c_str(), device.name.c_str()) == 0;
        drawAlertIcon(device.audio ? IslandAlert::Kind::bluetoothAudio : IslandAlert::Kind::bluetooth,
                      pt(x + 1, top + 8), accent_);
        text(device.name, bodyFont_.get(), box(x + 23, top + (active ? 0 : 7), width - (device.battery ? 90 : 23), 18),
             rgb(inkColor));
        if (active) text(L"Audio output", bodyFont_.get(), box(x + 23, top + 16, width - 23, 16), rgb(mutedColor));
        if (device.battery) {
            drawBatteryCell(column.right - 62, top + 16, *device.battery, accent_);
            text(std::to_wstring(*device.battery) + L"%", bodyEndFont_.get(),
                 box(column.right - 36, top, 36, bluetoothRowHeight), accent_);
        }
    }
    const auto notificationLabel = !windowsNotificationsQuiet_ ? L"Notification settings" :
        *windowsNotificationsQuiet_ ? L"Do Not Disturb on" : L"Do Not Disturb off";
    drawButton(box(x, viewport.top + static_cast<float>(std::clamp<size_t>(count, 1, visibleBluetoothRows)) * bluetoothRowHeight + 4,
                   width, 25), notificationLabel, Hit::notificationSettings, record);
}

void Island::drawPrayerSchedule(D2D1_RECT_F panel, bool record) {
    const float x = (panel.left + panel.right - prayerPanelWidth) / 2 + padX;
    const float y = panel.top + 12;
    const auto info = box(x, y, prayerInfoWidth, 115);
    const auto picker = box(x + 12, panel.top + prayerPickerTop, 137, prayerPickerHeight);
    target_->FillRoundedRectangle(rounded(info, 12), ink(withAlpha(accent_, 0.10f)));
    drawPickerButton(picker, L"Change location", Hit::prayerLocationPicker, record, prayerPickerOpen_);

    if (!prayerSchedule_) {
        text(L"Prayer times unavailable", bodyFont_.get(), box(x + 12, y + 12, prayerInfoWidth - 24, 25), rgb(mutedColor));
        return;
    }
    const auto& schedule = *prayerSchedule_;
    const auto timing = prayerProgress(schedule, std::chrono::system_clock::now());
    const auto name = std::wstring_view(prayerNames[timing.nextIndex]);
    constexpr float infoInnerWidth = prayerInfoWidth - 24;
    const float nameWidth = std::ceil(measure(name, prayerTitleFont_.get())) + 2;
    text(name, prayerTitleFont_.get(), box(x + 12, y + 8, nameWidth, 34), rgb(inkColor));

    const auto hours = std::chrono::duration_cast<std::chrono::hours>(timing.remaining);
    const auto minutes = timing.remaining - hours;
    std::wstring countdown = L"in ";
    if (hours.count()) countdown += std::to_wstring(hours.count()) + L"h ";
    countdown += std::to_wstring(minutes.count()) + L"m";
    text(countdown, bodyFont_.get(), box(x + 19 + nameWidth, y + 15, infoInnerWidth - nameWidth - 7, 26), accent_);

    text(std::wstring(L"Prayer times · ") + schedule.city, bodyFont_.get(),
         box(x + 12, y + 42, infoInnerWidth, 17), rgb(mutedColor));
    auto clock = std::wstring(L"Now ") + prayerTimeText(static_cast<int>(timing.localMinute)) + L" " + schedule.zone;
    text(clock, bodyFont_.get(), box(x + 12, y + 62, infoInnerWidth, 17), rgb(inkColor));
    if (timing.tomorrow)
        text(L"Tomorrow", bodyEndFont_.get(), box(x + 12, y + 62, infoInnerWidth, 17), accent_);

    const float scheduleLeft = info.right + 15;
    const float scheduleRight = x + prayerPanelWidth - 2 * padX;
    const float left = scheduleLeft + 10;
    const float right = scheduleRight - 10;
    const float baseline = y + 55;
    constexpr float rise = 23;
    constexpr float dayMinutes = 24 * 60;
    const auto pointAt = [&](float t) {
        return pt(std::lerp(left, right, t), baseline - 4 * rise * t * (1 - t));
    };
    const auto drawArc = [&](float t, D2D1_COLOR_F color) {
        if (t <= 0) return;
        const auto end = pointAt(t);
        const float width = end.x - left;
        PathBuilder arc(graphics_.get());
        arc.move(pt(left, baseline), false)
            .curve(pt(left + width / 3, baseline - 4 * rise * t / 3),
                   pt(end.x - width / 3, end.y - 4 * rise * (2 * t - 1) * t / 3), end);
        target_->DrawGeometry(arc.finish().get(), ink(color), 1.4f, roundStroke_.get());
    };
    drawArc(1, white(0.30f));
    const float nowFraction = static_cast<float>(timing.localMinute) / dayMinutes;
    drawArc(nowFraction, withAlpha(accent_, 0.62f));

    const float labelStep = (scheduleRight - scheduleLeft) / static_cast<float>(prayerNames.size());
    for (size_t i = 0; i < schedule.times.size(); ++i) {
        const auto point = pointAt(static_cast<float>(schedule.times[i]) / dayMinutes);
        const bool upcoming = !timing.tomorrow && i == timing.nextIndex;
        if (upcoming) {
            const auto ring = D2D1::Ellipse(point, 5.5f, 5.5f);
            target_->FillEllipse(ring, ink(rgb(0x10161e)));
            target_->DrawEllipse(ring, ink(accent_), 1.4f);
        }
        target_->FillEllipse(D2D1::Ellipse(point, 2.6f, 2.6f), ink(upcoming ? accent_ : rgb(inkColor)));
        const auto row = box(scheduleLeft + static_cast<float>(i) * labelStep + 2, y + 77, labelStep - 4, 38);
        if (upcoming) {
            target_->FillRoundedRectangle(rounded(row, 7), ink(withAlpha(accent_, 0.14f)));
            target_->DrawRoundedRectangle(rounded(row, 7), ink(withAlpha(accent_, 0.35f)), 1);
        }
        text(prayerNames[i], buttonFont_.get(), box(row.left, row.top + 3, row.right - row.left, 14),
             upcoming ? accent_ : rgb(mutedColor));
        text(prayerTimeText(schedule.times[i]), prayerTimeFont_.get(), box(row.left, row.top + 17, row.right - row.left, 18),
             upcoming ? accent_ : rgb(inkColor));
    }
    const auto current = pointAt(nowFraction);
    fillPolygon({pt(current.x - 4, current.y - 14), pt(current.x + 4, current.y - 14),
                 pt(current.x, current.y - 7)}, accent_);

    const float axisY = y + 58;
    text(L"00:00", monoFont_.get(), box(scheduleLeft + 4, axisY, 40, 15), rgb(mutedColor));
    text(L"12:00", buttonFont_.get(), box((scheduleLeft + scheduleRight) / 2 - 20, axisY, 40, 15), rgb(mutedColor));
    text(L"24:00", monoEndFont_.get(), box(scheduleRight - 44, axisY, 40, 15), rgb(mutedColor));
}

void Island::drawPrayerLocationMenu(D2D1_RECT_F panel) {
    const float x = (panel.left + panel.right - prayerPanelWidth) / 2 + padX + 12;
    const float y = panel.top + prayerMenuTop;
    constexpr float menuWidth = 137;
    const auto menu = box(x, y, menuWidth, pickerHeight(prayerLocations.size()));
    drawPickerSurface(menu, Hit::prayerLocationMenu);
    for (size_t i = 0; i < prayerLocations.size(); ++i) {
        const auto row = box(x + pickerInset, y + pickerInset + pickerRowHeight * static_cast<float>(i),
                             menuWidth - 2 * pickerInset, pickerRowHeight - 1);
        drawPickerRow(row, prayerLocations[i].name, Hit::prayerLocation, i + 1, preferences_.prayerCity == i + 1);
    }
}

void Island::drawAppVolume(size_t index, D2D1_RECT_F row, bool record) {
    const auto& app = appVolumes_[index];
    const bool dragged = !volumeDragId_.empty() && volumeDragId_ == app.id;
    const bool muted = app.muted && !dragged;
    const float level = dragged ? volumeDragValue_ : app.volume;
    const float middle = (row.top + row.bottom) / 2;

    // App icon; click to mute. A muted app's icon dims and gets a slash.
    const auto tile = box(row.left, middle - 12, 24, 24);
    if (hovered(Hit::appMute, index) || pressed(Hit::appMute, index)) {
        target_->FillRoundedRectangle(rounded(tile, 6), ink(white(0.1f)));
    }
    const float iconOpacity = muted ? 0.4f : 1.0f;
    const auto glyph = box(tile.left + 3, tile.top + 3, 18, 18);
    auto* logo = app.iconId.empty() ? nullptr : icon(app.iconId);
    if (!logo && app.id != L"system") logo = icon(app.id);
    if (app.id == L"system") {
        drawAudioGlyph(pt(tile.left + 12, middle), false, withAlpha(rgb(inkColor), iconOpacity));
    } else if (logo && bitmap(*logo)) {
        target_->DrawBitmap(logo->bitmap.get(), glyph, iconOpacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    } else {
        target_->FillRoundedRectangle(rounded(glyph, 5), ink(withAlpha(accent_, iconOpacity)));
        text(app.name.substr(0, 1), letterFont_.get(), glyph, rgb(0x0b0d11));
    }
    if (muted) {
        target_->DrawLine(pt(tile.left + 4, tile.bottom - 4), pt(tile.right - 4, tile.top + 4), ink(rgb(0xff6961)),
                          1.6f, roundStroke_.get());
    }
    if (record) addTarget(tile, Hit::appMute, index);

    text(app.name, bodyFont_.get(), box(row.left + 28, row.top, 75, appRowHeight),
         rgb(muted ? mutedColor : inkColor));

    // Slider: a 4 DIP track with a 12 DIP thumb; the whole row height is draggable.
    const float trackLeft = row.left + 110;
    const float trackRight = row.right - 44;
    const float fill = trackLeft + (trackRight - trackLeft) * clamp01(level);
    const auto color = muted ? rgb(0x6b7280) : accent_;
    target_->FillRoundedRectangle(rounded(D2D1::RectF(trackLeft, middle - 2, trackRight, middle + 2), 2),
                                  ink(white(0.17f)));
    if (fill > trackLeft) {
        target_->FillRoundedRectangle(rounded(D2D1::RectF(trackLeft, middle - 2, fill, middle + 2), 2), ink(color));
    }
    const float thumb = dragged || hovered(Hit::appVolume, index) ? 7.0f : 6.0f;
    target_->FillEllipse(D2D1::Ellipse(pt(fill, middle), thumb, thumb), ink(color));
    if (record) addTarget(D2D1::RectF(trackLeft - 6, row.top, trackRight + 6, row.bottom), Hit::appVolume, index);

    const auto value = muted ? std::wstring(L"Muted") : std::to_wstring(std::lround(level * 100)) + L"%";
    text(value, bodyEndFont_.get(), box(row.right - 40, row.top, 40, appRowHeight), rgb(muted ? mutedColor : inkColor));
}

std::wstring_view Island::screenLabel(Screen screen) {
    switch (screen) {
    case Screen::media: return L"Now Playing";
    case Screen::timers: return L"Timers";
    case Screen::scratch: return L"Scratchpad";
    case Screen::drop: return L"Drop Zone";
    case Screen::system: return L"System";
    case Screen::prayer: return L"Prayer";
    }
    return {};
}

void Island::drawScreenGlyph(Screen screen, D2D1_RECT_F rect, D2D1_COLOR_F color) {
    const auto center = pt((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
    switch (screen) {
    case Screen::media: {
        const auto x = center.x, y = center.y;
        const auto brush = ink(color);
        target_->FillEllipse(D2D1::Ellipse(pt(x - 4.3f, y + 4.5f), 2.4f, 1.7f), brush);
        target_->FillEllipse(D2D1::Ellipse(pt(x + 3.3f, y + 2.5f), 2.4f, 1.7f), brush);
        PathBuilder notes(graphics_.get());
        notes.move(pt(x - 2.4f, y + 4.4f), false).line(pt(x - 2.4f, y - 4.4f))
            .line(pt(x + 5.2f, y - 6.5f)).line(pt(x + 5.2f, y + 2.4f));
        target_->DrawGeometry(notes.finish().get(), brush, 1.35f, roundStroke_.get());
        break;
    }
    case Screen::timers: drawTimerGlyph(center, color); break;
    case Screen::scratch: text(L"✎", glyphFont_.get(), rect, color); break;
    case Screen::drop:
        target_->DrawLine(pt(center.x, center.y - 7), pt(center.x, center.y + 1), ink(color), 1.5f);
        target_->DrawLine(pt(center.x - 3, center.y - 2), pt(center.x, center.y + 1), ink(color), 1.5f);
        target_->DrawLine(pt(center.x, center.y + 1), pt(center.x + 3, center.y - 2), ink(color), 1.5f);
        target_->DrawRoundedRectangle(rounded(box(center.x - 7, center.y + 3, 14, 4), 1), ink(color), 1.5f);
        break;
    case Screen::system: drawChipGlyph(center, color); break;
    case Screen::prayer: text(L"☾", glyphFont_.get(), rect, color); break;
    }
}

float Island::navExpansion(Clock::time_point now) const {
    const float t = animate_ ? easeInOut(progress(navStart_, navRevealTime, now)) : 1;
    return std::lerp(navFrom_, navExpanded_ ? 1.0f : 0.0f, t);
}

float Island::navHeightAt(Clock::time_point now) const {
    return std::lerp(compactNavHeight, navHeight, navExpansion(now));
}

bool Island::setNavExpanded(bool expanded, Clock::time_point now) {
    if (expanded == navExpanded_) return false;
    navFrom_ = navExpansion(now);
    navExpanded_ = expanded;
    navStart_ = now;
    return true;
}

float Island::navSelection(Clock::time_point now) const {
    const auto selected = std::ranges::find(screens_, screen_);
    const float target = selected == screens_.end() ? 0 : static_cast<float>(selected - screens_.begin());
    if (!animate_) return target;
    return std::lerp(navSelectionFrom_, target, easeInOut(progress(navSelectionStart_, navSelectionTime, now)));
}

void Island::drawNav(D2D1_RECT_F panel, Clock::time_point now, bool record) {
    const float top = panel.top + 3;
    const float height = navHeightAt(now);
    const auto amounts = captureAmounts(now);
    const auto [sharing, camera, mic] = amounts;
    const float captureRight = std::lerp(26.0f, 38.0f, lin(2, 3, sharing + camera + mic));
    drawCaptureIndicators(pt(panel.left + padX + captureRight, top + height / 2), amounts);
    const auto count = screens_.size();
    constexpr float expandedStep = 31;
    const float expandedWidth = expandedStep * static_cast<float>(count);
    const float centerX = (panel.left + panel.right) / 2;
    if (record && count) navHoverRect_ = box(centerX - expandedWidth / 2, panel.top, expandedWidth, navHeight + 3);
    const float reveal = navExpansion(now);
    const float step = std::lerp(12.0f, expandedStep, reveal);
    const float start = centerX - static_cast<float>(count) * step / 2;
    constexpr Hit hits[]{Hit::navMedia, Hit::navTimers, Hit::navScratch, Hit::navDrop, Hit::navSystem, Hit::navPrayer};
    const float selectionX = start + (navSelection(now) + 0.5f) * step;
    if (count && reveal > 0) {
        const auto selection = box(selectionX - (step - 4) / 2, top + 2, step - 4, height - 4);
        target_->FillRoundedRectangle(rounded(selection, 7), ink(withAlpha(accent_, 0.14f * reveal)));
    }
    for (size_t i = 0; i < count; ++i) {
        const auto cell = box(start + static_cast<float>(i) * step, top, step, height);
        const auto rect = grow(cell, -2, -2);
        const auto screen = screens_[i];
        const auto hit = hits[static_cast<unsigned>(screen)];
        const bool selected = screen_ == screen;
        if (!selected && hovered(hit)) target_->FillRoundedRectangle(rounded(rect, 7), ink(white(0.08f * reveal)));
        const auto color = selected ? accent_ : rgb(mutedColor);
        const auto center = pt((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
        if (reveal < 1) {
            const auto marker = box(center.x - 2.4f, center.y - 2.4f, 4.8f, 4.8f);
            target_->FillRoundedRectangle(rounded(marker, 2.4f), ink(rgb(mutedColor, 1 - reveal)));
        }
        if (reveal > 0) drawScreenGlyph(screen, rect, withAlpha(color, reveal));
        if (record) addTarget(cell, hit);
    }
    if (count && reveal < 1) {
        const auto marker = box(selectionX - 5.5f, top + height / 2 - 2.4f, 11, 4.8f);
        target_->FillRoundedRectangle(rounded(marker, 2.4f), ink(withAlpha(accent_, 1 - reveal)));
    }
    const auto pin = box(panel.right - padX - 26, top, 26, 24);
    if (hovered(Hit::panelPin) || pressed(Hit::panelPin)) {
        target_->FillRoundedRectangle(rounded(pin, 6), ink(white(pressed(Hit::panelPin) ? 0.14f : 0.09f)));
    }
    if (record) addTarget(pin, Hit::panelPin);
    const bool pinned = panelPinned();
    const auto color = pinned ? accent_ : rgb(inkColor);
    const float x = (pin.left + pin.right) / 2, y = (pin.top + pin.bottom) / 2;
    PathBuilder glyph(graphics_.get());
    glyph.move(pt(x - 4.5f, y - 7), pinned).line(pt(x + 4.5f, y - 7)).line(pt(x + 2.5f, y - 4))
        .line(pt(x + 2.5f, y - 1)).line(pt(x + 5, y + 2)).line(pt(x - 5, y + 2))
        .line(pt(x - 2.5f, y - 1)).line(pt(x - 2.5f, y - 4)).close();
    const auto shape = glyph.finish();
    if (pinned) target_->FillGeometry(shape.get(), ink(color));
    else target_->DrawGeometry(shape.get(), ink(color), 1.4f, roundStroke_.get());
    target_->DrawLine(pt(x, y + 2), pt(x, y + 7), ink(color), 1.4f, roundStroke_.get());
}

void Island::drawSharedTitle(D2D1_RECT_F compact, D2D1_RECT_F panel, float value) {
    // The title is drawn at its panel size and scaled down to the pill's 12px at the start.
    constexpr float compactScale = 12.0f / mediaTitleSize;
    const float scale = std::lerp(compactScale, 1.0f, value);
    const float x = std::lerp(compact.left + pillTrackLeft, panel.left + mediaTitleLeft, value);
    const float y = std::lerp((compact.top + compact.bottom - mediaTitleHeight * compactScale) / 2,
                              panel.top + mediaTitleTop, value);
    const float compactWidth = compact.right - compact.left - pillTrackLeft - pillTrackRight;
    const float width = std::lerp(compactWidth / compactScale, mediaTitleWidth, value);
    target_->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) * D2D1::Matrix3x2F::Translation(x, y) * base_);
    text(title(), mediaTitleFont_.get(), box(0, 0, width, mediaTitleHeight), rgb(inkColor));
    target_->SetTransform(base_);
}

// ---- Alerts --------------------------------------------------------------------------------------

bool Island::alertsQuiet() const {
    return preferences_.quietMode == QuietMode::on ||
        (preferences_.quietMode == QuietMode::followWindows && windowsNotificationsQuiet_.value_or(false));
}

bool Island::alertAllowed(IslandAlert::Kind kind) const {
    if (isCaptureAlert(kind)) return false;
    if (kind == IslandAlert::Kind::mediaStarted) return preferences_.enabled(Screen::media);
    if (alertsQuiet()) return false;
    switch (kind) {
    case IslandAlert::Kind::copy: return preferences_.showClipboardBubble;
    case IslandAlert::Kind::focusDone:
    case IslandAlert::Kind::breakDone:
    case IslandAlert::Kind::timerDone: return preferences_.enabled(Screen::timers);
    default: return true;
    }
}

void Island::notify(IslandAlert alert) {
    if (suppressed_ || !alertAllowed(alert.kind)) return;
    const auto now = Clock::now();
    prepareText();
    const bool replacesSameKind = alert.kind == IslandAlert::Kind::copy || alert.kind == IslandAlert::Kind::connection ||
                                  alert.kind == IslandAlert::Kind::capsLock || alert.kind == IslandAlert::Kind::numLock;
    if (alert_ && alert_->kind == alert.kind && replacesSameKind && !closingDot_) {
        if (alertInline_) {
            alertUntil_ = now + alertHoldTime(alert);
        } else if (alertTo_ == 0) {
            const auto motion = alertMotion(now);
            alertFrom_ = motion.value;
            alertVelocity_ = motion.velocity;
            alertTo_ = 1;
            alertStart_ = now;
        } else if (alertHoldUntil_) {
            alertHoldUntil_ = now + alertHoldTime(alert);
        }
        startAlert(std::move(alert), now, true);
        return;
    }
    if (alert_) {
        if (!queued_ || alert.priority >= queued_->priority) queued_ = std::move(alert);
        return;
    }
    startAlert(std::move(alert), now, false);
}

void Island::clearAlerts() {
    alert_.reset();
    queued_.reset();
    alertHoldUntil_.reset();
    closingDot_.reset();
    alertFrom_ = alertTo_ = 0;
}

void Island::startAlert(IslandAlert alert, Clock::time_point now, bool update) {
    float width = 12 + 15 + 12;
    if (!alert.title.empty()) width += 7 + measure(alert.title, alertTitleFont_.get());
    if (!alert.value.empty()) width += 7 + measure(alert.value, alertValueFont_.get());
    if (alert.battery) width += batteryCellWidth;
    alertWidth_ = std::clamp(width + 1, 52.0f, maxBubble);
    alert_ = std::move(alert);
    if (update) return;
    closingDot_.reset();
    const bool hasPrimaryPill = pillMode_ != PillMode::dot && pillMode_ != PillMode::privacy;
    const bool hasCompanion = activityCompanion() != PillMode::dot;
    alertUsesCompanionSlot_ = hasPrimaryPill && alert_->kind != IslandAlert::Kind::mediaStarted &&
        !alert_->dotOrigin && (alert_->fromCompanion || hasCompanion);
    privacyDetailsOpen_ = false;
    const bool closing = !open_ && panelValue(now) > 0;
    if (!closing && !pillOccluded_) {
        // The split starts from a settled dot or pill.
        pillFrom_ = pillTarget_;
        pillVelocity_ = 0;
        pillStart_ = {};
    }
    alertHoldUntil_.reset();
    alertInline_ = open_;
    if (alertInline_) {
        alertUntil_ = now + alertHoldTime(*alert_);
        return;
    }
    pillSizeFrom_ = pillSizeTarget_;
    pillTransitionStart_ = {};
    previousPillLyric_ = {};
    openAt_.reset();
    alertFrom_ = 0;
    alertVelocity_ = 0;
    alertTo_ = 1;
    alertStart_ = closing ? std::max(now, panelStart_ + shapeTime) : now;
}

void Island::beginAlertClose(Clock::time_point now) {
    alertFrom_ = 1;
    alertVelocity_ = 0;
    alertTo_ = 0;
    alertStart_ = now;
    alertHoldUntil_.reset();

    // Activity-ending bubbles that grow from a dot return along the primary pill's split.
    if (alert_->dotOrigin) return;

    AlertDot destination;
    destination.mode = activityCompanion();
    const bool hasCapture = std::ranges::any_of(captureIndicators_, [](auto const& indicator) {
        return !indicator.app.empty();
    });
    if (pillMode_ == PillMode::dot && !hasCapture && pillAmount(now) >= 1) {
        destination.idle = true;
        closingDot_ = destination;
    } else if (pillMode_ != PillMode::dot && pillMode_ != PillMode::privacy &&
               destination.mode != PillMode::dot) {
        closingDot_ = destination;
    }
}

void Island::finishAlert(Clock::time_point now) {
    const auto dot = std::exchange(closingDot_, std::nullopt);
    alert_.reset();
    alertFrom_ = alertTo_ = 0;
    alertHoldUntil_.reset();
    updateCaptureIndicators(now);
    updatePill(now);
    if (dot && dot->idle && pillMode_ == PillMode::dot) {
        pillFrom_ = pillTarget_ = 0;
        pillVelocity_ = 0;
        pillStart_ = {};
    }
    if (queued_) {
        auto next = std::move(*queued_);
        queued_.reset();
        if (next.kind != IslandAlert::Kind::mediaStarted || activityCompanion() == PillMode::media)
            startAlert(std::move(next), now, false);
    }
}

Island::MotionSample Island::alertMotion(Clock::time_point now) const {
    if (!animate_) return {alertTo_, 0};
    if (alertFrom_ == 1 && alertTo_ == 0 && alertVelocity_ == 0)
        return reverseSplitMotion(alertStart_, now);
    return sampleSpring(alertStart_, splitTime, now, alertFrom_, alertTo_, alertVelocity_, Spring::split);
}

Island::MotionSample Island::reverseSplitMotion(Clock::time_point start, Clock::time_point now) {
    const auto duration = std::chrono::duration_cast<Clock::duration>(splitTime);
    const auto elapsed = std::clamp(now - start, Clock::duration::zero(), duration);
    const auto opening = sampleSpring(start, duration, start + duration - elapsed, 0, 1, 0, Spring::split);
    return {opening.value, -opening.velocity};
}

float Island::alertValue(Clock::time_point now) const {
    return alertMotion(now).value;
}

bool Island::bubbleShowing() const {
    return alert_ && !alertInline_;
}

float Island::inlineInset() const {
    return alert_ && alertInline_ ? 23.0f : 0.0f;
}

float Island::measure(std::wstring_view value, IDWriteTextFormat* format) {
    winrt::com_ptr<IDWriteTextLayout> layout;
    winrt::check_hresult(fonts_->CreateTextLayout(value.data(), static_cast<UINT32>(value.size()), format, 1000, 40,
                                                  layout.put()));
    DWRITE_TEXT_METRICS metrics{};
    winrt::check_hresult(layout->GetMetrics(&metrics));
    return metrics.widthIncludingTrailingWhitespace;
}

D2D1_RECT_F Island::closingDotShape(Clock::time_point now, D2D1_RECT_F pill) const {
    if (closingDot_->idle) return box(-dotSize / 2, pillCenterY - dotSize / 2, dotSize, dotSize);
    const float top = (pill.top + pill.bottom - activityDotSize) / 2;
    return box(pill.right + activityDotGap, top, activityWidth(closingDot_->mode, now), activityDotSize);
}

void Island::drawBubbleToDot(Clock::time_point now, D2D1_RECT_F pill, D2D1_RECT_F bubble, bool fromSlot) {
    const float progress = 1 - alertValue(now);
    if (!fromSlot || closingDot_->idle) {
        const float width = pill.right - pill.left;
        const float height = pill.bottom - pill.top;
        const float gap = releaseGap * height / pillHeight;
        const float splitLeft = fromSlot ? pill.left : bubble.left - gap - width;
        const auto main = box(std::lerp(splitLeft, pill.left, progress), pill.top, width, height);
        const float opacity = closingDot_->idle ? 1 - easeInOut(lin(0, 0.7f, progress)) : 1;
        drawBubblePrimary(now, main, opacity);
    }
    const auto shape = mixRect(bubble, closingDotShape(now, pill), progress);
    const float pinch = std::sin(pi * progress) * 0.28f;
    drawCompanionShell(shape, pinch, 0.13f);
    drawAlertContent(shape, 1 - easeInOut(lin(0.05f, 0.45f, progress)), 1);
    if (!closingDot_->idle && progress > 0.3f) {
        pushLayer(nullptr, easeInOut(lin(0.3f, 0.9f, progress)));
        drawActivityIndicator(now, shape, closingDot_->mode, rgb(inkColor));
        popLayer();
    }
}

void Island::drawBubblePrimary(Clock::time_point now, D2D1_RECT_F pill, float opacity) {
    if (opacity <= 0) return;
    const auto shell = rounded(pill, (pill.bottom - pill.top) / 2);
    winrt::com_ptr<ID2D1RoundedRectangleGeometry> clip;
    winrt::check_hresult(graphics_->CreateRoundedRectangleGeometry(shell, clip.put()));
    pushLayer(clip.get(), opacity);
    fillShell(shell);
    drawCompactContent(now, pill, false);
    popLayer();
    target_->DrawRoundedRectangle(shell, ink(white(0.13f * opacity)), 1);
}

void Island::drawBubbleFromDot(Clock::time_point now, D2D1_RECT_F pill, D2D1_RECT_F bubble) {
    const auto& origin = *alert_->dotOrigin;
    const float expansion = alertValue(now);
    const float height = pill.bottom - pill.top;
    const float width = pill.right - pill.left;
    const float gap = releaseGap * height / pillHeight;
    const auto primary = box(std::lerp(pill.left, bubble.left - gap - width, expansion), pill.top, width, height);
    drawBubblePrimary(now, primary, 1);

    const float top = (pill.top + pill.bottom - activityDotSize) / 2;
    const auto dot = box(pill.right + activityDotGap, top, origin.width, activityDotSize);
    auto shape = mixRect(dot, bubble, expansion);
    if (animate_ && isCountdownAlert(alert_->kind))
        shape = countdownShake(shape, progress(alertStart_, countdownCompletionMotion, now));
    drawCompanionShell(shape, 0, 0.13f);
    const float textOpacity = easeInOut(lin(0.55f, 1, expansion));
    const float notice = isCountdownAlert(alert_->kind) ? easeInOut(lin(0, 0.35f, expansion)) : 1;
    if (notice < 1) {
        const auto mode = alert_->kind == IslandAlert::Kind::timerDone ? PillMode::timer : PillMode::focus;
        pushLayer(nullptr, 1 - notice);
        drawActivityIndicator(now, shape, mode, rgb(inkColor));
        popLayer();
    }
    drawAlertContent(shape, notice, textOpacity, shape.left + origin.iconLeft);
}

void Island::drawSplit(Clock::time_point now, float amount, std::optional<D2D1_RECT_F> slot) {
    const float p = alertValue(now);
    const float target = alertWidth_;
    PathBuilder shape(graphics_.get());
    float left = 0;
    D2D1_RECT_F area{};
    float alertOpacity = 0;
    float copyOpacity = 0;
    const bool fromPill = amount >= 1;
    const auto source = slot.value_or(compactShape(now, amount));
    const auto size = D2D1::SizeF(source.right - source.left, source.bottom - source.top);
    const float scale = fromPill ? size.height / pillHeight : 1;
    const float shiftX = slot ? slot->left : 0;
    const float top = slot ? slot->top : pillTop;
    if (fromPill) {
        const auto frame = makeLiquidSplit(shape, size.width / scale, target / scale, p, slot.has_value());
        left = frame.left;
        area = frame.bubble;
        alertOpacity = lin(pinchAt - 0.1f, pinchAt + 0.12f, p);
        copyOpacity = lin(0.8f, 1, p);
    } else {
        // From the idle dot the bubble grows out of the dot itself.
        const float width = std::lerp(dotSize, target, p);
        const float height = std::lerp(dotSize, activityDotSize, p);
        area = box(-width / 2, pillCenterY - height / 2, width, height);
        capsule(shape, area);
        const float t = lin(0.31f, 0.82f, p);
        alertOpacity = t * t * (3 - 2 * t);
        copyOpacity = 1;
    }
    const auto geometry = shape.finish();
    winrt::com_ptr<ID2D1TransformedGeometry> stretched;
    ID2D1Geometry* visible = geometry.get();
    if (scale != 1 || slot) {
        const auto transform = D2D1::Matrix3x2F::Scale(scale, scale) *
            D2D1::Matrix3x2F::Translation(shiftX, top - pillTop * scale);
        winrt::check_hresult(graphics_->CreateTransformedGeometry(geometry.get(), &transform, stretched.put()));
        visible = stretched.get();
        area = box(shiftX + area.left * scale, top, (area.right - area.left) * scale, size.height);
    }
    const bool drawsWholePill = !slot;
    if (!drawsWholePill) {
        // A small companion bubble draws only the part beyond the primary pill.
        target_->PushAxisAlignedClip(D2D1::RectF(slot->right, slot->top - 1,
                                                 slot->right + releaseGap * scale + std::max(target, size.height) + 2,
                                                 slot->bottom + 1), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }
    fillShell(visible);
    target_->DrawGeometry(visible, ink(white(0.13f)), 1);
    if (!drawsWholePill) target_->PopAxisAlignedClip();

    if (fromPill && drawsWholePill) {
        const auto capsule = box(shiftX + left * scale, top, size.width, size.height);
        winrt::com_ptr<ID2D1RoundedRectangleGeometry> clip;
        winrt::check_hresult(graphics_->CreateRoundedRectangleGeometry(
            rounded(capsule, size.height / 2), clip.put()));
        pushLayer(clip.get(), 1);
        drawCompactContent(now, capsule, false);
        popLayer();
    }
    drawAlertContent(area, alertOpacity, copyOpacity);
}

void Island::drawAlertContent(D2D1_RECT_F area, float opacity, float textOpacity,
                              std::optional<float> iconOrigin) {
    if (opacity <= 0 || !alert_) return;
    const float width = area.right - area.left;
    const bool copied = alert_->kind == IslandAlert::Kind::copy;
    const float contentWidth = alertWidth_;
    const float contentLeft = area.left + (width - contentWidth) / 2;
    const float iconProgress = iconOrigin ? textOpacity : easeInOut(lin(contentWidth - 24, contentWidth - 6, width));
    // Lay out the complete label once. The final stretch controls its opacity and focus.
    const float revealWidth = contentWidth - 0.5f;
    const float reveal = textOpacity * easeInOut(lin(revealWidth - 16, revealWidth, width));
    const float radius = std::min(width, area.bottom - area.top) / 2;
    winrt::com_ptr<ID2D1RoundedRectangleGeometry> clip;
    winrt::check_hresult(graphics_->CreateRoundedRectangleGeometry(rounded(area, radius), clip.put()));
    pushLayer(clip.get(), opacity);
    const float middle = (area.top + area.bottom) / 2;
    // While the bubble is narrow the icon sits centered; the text fades in as it reaches full width.
    const float iconLeft = std::lerp(iconOrigin.value_or(area.left + std::max(0.0f, (width - 15) / 2)),
                                     contentLeft + 12, iconProgress);
    drawAlertIcon(alert_->kind, pt(iconLeft, middle - 7.5f), alert_->color ? rgb(*alert_->color) : accent_);
    if (reveal <= 0) {
        popLayer();
        return;
    }
    float x = contentLeft + 12 + 15 + 7;
    const float end = contentLeft + contentWidth - 12;
    // A long title (a device name) gives way so the value after it stays readable.
    float valueWidth = 0;
    if (!copied && !alert_->value.empty()) {
        valueWidth = 7 + measure(alert_->value, alertValueFont_.get()) + (alert_->battery ? batteryCellWidth : 0);
    }
    if (!alert_->title.empty() && end > x) {
        const float measuredTitle = measure(alert_->title, alertTitleFont_.get());
        const float titleWidth = copied ? std::ceil(measuredTitle) + 2 :
            std::min(measuredTitle + 1, std::max(0.0f, end - x - valueWidth));
        text(alert_->title, alertTitleFont_.get(), box(x, middle - 10, titleWidth, 20), withAlpha(rgb(inkColor), reveal));
        x += titleWidth + 6;
    }
    if (!alert_->value.empty() && end > x) {
        auto color = rgb(mutedColor);
        if (alert_->battery && end - x > batteryCellWidth) {
            color = rgb(batteryColor(*alert_->battery));
            drawBatteryCell(x, middle, *alert_->battery, withAlpha(color, reveal));
            x += batteryCellWidth;
        }
        text(alert_->value, alertValueFont_.get(), box(x, middle - 10, end - x, 20), withAlpha(color, reveal));
    }
    popLayer();
}

// The V7 alert glyphs, drawn in a 16-unit box scaled to 15 DIPs.
void Island::drawAlertIcon(IslandAlert::Kind kind, D2D1_POINT_2F origin, D2D1_COLOR_F color, float size) {
    const float scale = size / 16;
    target_->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) * D2D1::Matrix3x2F::Translation(origin.x, origin.y) *
                          base_);
    switch (kind) {
    case IslandAlert::Kind::capsLock:
    case IslandAlert::Kind::numLock:
        target_->DrawRoundedRectangle(rounded(box(1.5f, 1.5f, 13, 13), 2.5f), ink(color), 1.4f);
        text(kind == IslandAlert::Kind::capsLock ? L"A" : L"1", buttonFont_.get(), box(2, 2, 12, 12), color);
        break;
    case IslandAlert::Kind::copy: {
        target_->DrawRoundedRectangle(rounded(box(5, 5, 8.5f, 8.5f), 1.5f), ink(color), 1.5f);
        PathBuilder back(graphics_.get());
        back.move(pt(11, 3.25f), false).line(pt(11, 3)).arc(pt(9.5f, 1.5f), 1.5f, false, false).line(pt(3, 1.5f))
            .arc(pt(1.5f, 3), 1.5f, false, false).line(pt(1.5f, 9.5f)).arc(pt(3, 11), 1.5f, false, false)
            .line(pt(3.25f, 11));
        target_->DrawGeometry(back.finish().get(), ink(color), 1.5f, roundStroke_.get());
        break;
    }
    case IslandAlert::Kind::charging:
        fillPolygon({pt(9, 1.5f), pt(3.5f, 9), pt(7.5f, 9), pt(7, 14.5f), pt(12.5f, 7), pt(8.5f, 7)}, color);
        break;
    case IslandAlert::Kind::battery:
    case IslandAlert::Kind::lowBattery:
        target_->DrawRoundedRectangle(rounded(box(1.5f, 4.5f, 11.5f, 7), 1.5f), ink(color), 1.5f);
        target_->DrawLine(pt(14.75f, 7), pt(14.75f, 9), ink(color), 1.5f, roundStroke_.get());
        target_->FillRoundedRectangle(
            rounded(box(3, 6, kind == IslandAlert::Kind::lowBattery ? 2.0f : 7.0f, 4), 0.5f), ink(color));
        break;
    case IslandAlert::Kind::focusDone:
    case IslandAlert::Kind::breakDone:
        drawFocusGlyph(pt(8, 8), 16, color);
        break;
    case IslandAlert::Kind::timerDone:
        drawTimerGlyph(pt(8, 8), color);
        break;
    case IslandAlert::Kind::microphone: {
        target_->DrawRoundedRectangle(rounded(box(6, 1.5f, 4, 8), 2), ink(color), 1.5f);
        PathBuilder base(graphics_.get());
        base.move(pt(3.5f, 7.5f), false).line(pt(3.5f, 8.5f)).arc(pt(12.5f, 8.5f), 4.5f, false, false)
            .line(pt(12.5f, 7.5f));
        target_->DrawGeometry(base.finish().get(), ink(color), 1.5f, roundStroke_.get());
        target_->DrawLine(pt(8, 13), pt(8, 15), ink(color), 1.5f, roundStroke_.get());
        break;
    }
    case IslandAlert::Kind::camera: {
        target_->DrawRoundedRectangle(rounded(box(1.5f, 4, 9, 8), 1.5f), ink(color), 1.5f);
        PathBuilder lens(graphics_.get());
        lens.move(pt(10.5f, 6.5f), false).line(pt(14.5f, 4.5f)).line(pt(14.5f, 11.5f)).line(pt(10.5f, 9.5f));
        target_->DrawGeometry(lens.finish().get(), ink(color), 1.5f, roundStroke_.get());
        break;
    }
    case IslandAlert::Kind::screenCapture:
        target_->DrawRoundedRectangle(rounded(box(1, 2.5f, 14, 9), 1.5f), ink(color), 1.5f);
        target_->DrawLine(pt(8, 11.5f), pt(8, 14), ink(color), 1.5f, roundStroke_.get());
        target_->DrawLine(pt(5, 14), pt(11, 14), ink(color), 1.5f, roundStroke_.get());
        break;
    case IslandAlert::Kind::bluetoothAudio: {
        PathBuilder band(graphics_.get());
        band.move(pt(3, 10), false).line(pt(3, 8)).arc(pt(13, 8), 5, false, true).line(pt(13, 10));
        target_->DrawGeometry(band.finish().get(), ink(color), 1.5f, roundStroke_.get());
        target_->FillRoundedRectangle(rounded(box(2, 9.5f, 3, 4.5f), 1.2f), ink(color));
        target_->FillRoundedRectangle(rounded(box(11, 9.5f, 3, 4.5f), 1.2f), ink(color));
        break;
    }
    case IslandAlert::Kind::bluetooth: {
        PathBuilder rune(graphics_.get());
        rune.move(pt(4.5f, 5), false).line(pt(11.5f, 11)).line(pt(8, 14)).line(pt(8, 2)).line(pt(11.5f, 5))
            .line(pt(4.5f, 11));
        target_->DrawGeometry(rune.finish().get(), ink(color), 1.5f, roundStroke_.get());
        break;
    }
    case IslandAlert::Kind::prayer: {
        PathBuilder crescent(graphics_.get());
        crescent.move(pt(10.5f, 2), false).arc(pt(13.5f, 11), 6.2f, true, false)
            .arc(pt(10.5f, 2), 5.4f, false, true);
        target_->DrawGeometry(crescent.finish().get(), ink(color), 1.5f, roundStroke_.get());
        break;
    }
    case IslandAlert::Kind::connection: {
        PathBuilder signal(graphics_.get());
        signal.move(pt(1.5f, 5), false).curve(pt(5, 1.5f), pt(11, 1.5f), pt(14.5f, 5));
        signal.move(pt(4, 8), false).curve(pt(6, 6), pt(10, 6), pt(12, 8));
        signal.move(pt(6, 10.5f), false).curve(pt(7, 9.5f), pt(9, 9.5f), pt(10, 10.5f));
        target_->DrawGeometry(signal.finish().get(), ink(color), 1.4f, roundStroke_.get());
        target_->FillEllipse(D2D1::Ellipse(pt(8, 13.3f), 1.1f, 1.1f), ink(color));
        break;
    }
    case IslandAlert::Kind::mediaStarted:
        drawScreenGlyph(Screen::media, box(0, 0, 16, 16), color);
        break;
    case IslandAlert::Kind::notification: {
        PathBuilder bell(graphics_.get());
        bell.move(pt(2, 11.5f), false).line(pt(4, 9)).line(pt(4, 6))
            .arc(pt(12, 6), 4, false, true).line(pt(12, 9)).line(pt(14, 11.5f)).line(pt(2, 11.5f));
        target_->DrawGeometry(bell.finish().get(), ink(color), 1.5f, roundStroke_.get());
        target_->FillEllipse(D2D1::Ellipse(pt(8, 14), 1, 1), ink(color));
        break;
    }
    }
    target_->SetTransform(base_);
}

void Island::drawBatteryCell(float left, float middle, unsigned level, D2D1_COLOR_F color, bool charging) {
    const auto cell = box(left + 0.5f, middle - 5, 19, 10);
    target_->DrawRoundedRectangle(rounded(cell, 2.5f), ink(color), 1);
    target_->FillRoundedRectangle(rounded(box(cell.right + 1, middle - 2, 1.5f, 4), 0.75f), ink(color));
    if (charging) {
        const float center = cell.left + 9.5f;
        fillPolygon({pt(center + 0.9f, middle - 3.8f), pt(center - 3, middle + 0.4f),
                     pt(center - 0.4f, middle + 0.4f), pt(center - 0.9f, middle + 3.8f),
                     pt(center + 3, middle - 0.6f), pt(center + 0.4f, middle - 0.6f)}, color);
        return;
    }
    const float fill = 15 * std::min(level, 100u) / 100.0f;
    if (fill > 0.5f) target_->FillRoundedRectangle(rounded(box(cell.left + 2, middle - 3, fill, 6), 1), ink(color));
}

// While the panel is open an alert shows as a row at its top instead of a bubble.
void Island::drawInlineAlert(D2D1_RECT_F panel) {
    const float y = panel.top + padTop;
    drawAlertIcon(alert_->kind, pt(panel.left + padX, y), alert_->color ? rgb(*alert_->color) : accent_);
    auto line = alert_->title;
    if (!line.empty() && !alert_->value.empty()) line += L" ";
    line += alert_->value;
    text(line, alertValueFont_.get(), box(panel.left + padX + 21, y - 2, innerWidth - 21, 19), rgb(inkColor));
}

// ---- Frame ---------------------------------------------------------------------------------------

void Island::draw(HWND window) {
    const auto now = Clock::now();
    updateTheme(now);
    scale_ = GetDpiForWindow(window) / 96.0f * preferences_.islandScale();
    const auto monitor = MonitorFromPoint(POINT{}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(MONITORINFO);
    winrt::check_bool(GetMonitorInfoW(monitor, &monitorInfo));
    const auto work = monitorInfo.rcWork;
    const float workWidth = (work.right - work.left) / scale_;
    lyricsWidth_ = std::clamp(workWidth - mediaPanelWidth - 16.0f,
                              130.0f, maxLyricsWidth);
    const float opacity = visibility(now);
    anchor_ = {work.left + (work.right - work.left) / 2, work.top - std::lround((1 - opacity) * 14 * scale_)};
    prepareText();
    updatePillLayout(now, workWidth);
    const bool compactForControls = pillOccluded_ && pillMode_ != PillMode::preview;
    const bool swapping = !compactForControls && activitySwapActive(now);
    if (swapping) {
        pillTransitionStart_ = {};
        previousPillLyric_ = {};
    }

    const float amount = pillAmount(now);
    const float value = panelValue(now);
    const bool panelVisible = open_ || value > 0;
    const bool switchingScreen = screenFrom_ != screen_ && progress(screenStart_, screenFadeTime, now) < 1;
    const bool morphTitle = privacyBadgeWidth(now) <= 0.01f && shareTitle_ && screen_ == Screen::media && preferences_.enabled(Screen::media) && media_.hasSession &&
                            pillTitleVisible_ && pillTrackSize_.height <= pillHeight &&
                            !pillLyric_.layout && !previousPillLyric_.layout &&
                            !switchingScreen && value > 0 && value < 1;
    if (!panelVisible || panelWidthAt(now) <= mediaPanelWidth) lyricLayout_ = nullptr;

    // The surface only covers what is drawn this frame.
    const auto compact = compactShape(now, amount);
    auto bounds = unite(grow(compact, 2, 2), compactHit(compact, amount, compactForControls));
    if (panelVisible) bounds = unite(bounds, grow(panelRect(now), 2, 2));
    const bool splitVisible = !compactForControls && !panelVisible && bubbleShowing();
    const auto companion = activityCompanion();
    const bool slotBubble = splitVisible && amount >= 1 && alertUsesCompanionSlot_;
    const bool directClose = splitVisible && closingDot_.has_value();
    D2D1_RECT_F fullBubble{};
    std::optional<D2D1_RECT_F> bubbleSlot;
    if (slotBubble) {
        const float height = activityDotSize;
        const float top = (compact.top + compact.bottom - height) / 2;
        bubbleSlot = box(compact.right - height, top, height, height);
        const float gap = releaseGap * height / pillHeight;
        fullBubble = box(compact.right + gap, top, std::max(alertWidth_, height), height);
        bounds = unite(bounds, grow(fullBubble, gap + 2, 2));
    } else if (splitVisible) {
        const bool fromPill = amount >= 1;
        const float height = fromPill ? compact.bottom - compact.top : activityDotSize;
        const float bubble = std::max(alertWidth_, height);
        const float gap = fromPill ? releaseGap * height / pillHeight : 0;
        const float primaryWidth = fromPill ? compact.right - compact.left : 0;
        const float width = primaryWidth + gap + bubble;
        const float top = fromPill ? pillTop : pillCenterY - height / 2;
        fullBubble = box(-width / 2 + primaryWidth + gap, top, bubble, height);
        bounds = unite(bounds, grow(box(-width / 2, top, width, height), 2, 2));
    }
    if (directClose) bounds = unite(bounds, grow(closingDotShape(now, compact), 2, 2));
    const bool growsFromDot = splitVisible && amount >= 1 && alertTo_ > 0 && alert_->dotOrigin;
    if (growsFromDot) {
        const float top = (compact.top + compact.bottom - activityDotSize) / 2;
        bounds = unite(bounds, grow(box(compact.right + activityDotGap, top, alert_->dotOrigin->width,
                                       activityDotSize), 2, 2));
    }
    const bool handoff = !panelVisible && !splitVisible && swapping && activitySwap_.retireOutgoing;
    const bool pendingHandoff = panelVisible && !open_ && swapping && activitySwap_.retireOutgoing;
    if (pendingHandoff) bounds = unite(bounds, grow(unite(activitySwap_.pill, activitySwap_.dot), 2, 2));
    const bool introducingMedia = splitVisible && alert_->kind == IslandAlert::Kind::mediaStarted;
    const float slotOpacity = directClose || growsFromDot || handoff || introducingMedia ? 0 :
        splitVisible ? companionSlotOpacity(now) : 1;
    std::optional<D2D1_RECT_F> activityDot;
    if (!panelVisible && amount >= 1 && companion != PillMode::dot && slotOpacity > 0) {
        const float centerY = (compact.top + compact.bottom) / 2;
        activityDot = box(compact.right + activityDotGap, centerY - activityDotSize / 2,
                          activityWidth(companion, now), activityDotSize);
        bounds = unite(bounds, grow(*activityDot, 3, 3));
    }
    lastActivityDot_ = activityDot;
    const bool drawSwap = !panelVisible && !splitVisible && swapping && !handoff && activityDot;
    const auto swapEnd = activityDot.value_or(activitySwap_.dot);
    if (drawSwap) {
        auto outgoing = unite(activitySwap_.pill, activitySwap_.dot);
        outgoing.bottom = std::max(outgoing.bottom, swapEnd.bottom) + activitySwapDrop;
        bounds = unite(bounds, grow(outgoing, 2, 2));
        bounds.top = std::min(bounds.top, std::min(activitySwap_.dot.top, compact.top) - activitySwapLift - 2);
    }
    if (handoff) {
        bounds = unite(bounds, grow(unite(unite(activitySwap_.pill, activitySwap_.dot), compact), 6, 6));
    }
    const bool dotExiting = !compactForControls && animate_ && !panelVisible && !splitVisible && !handoff && !activityDot &&
                            now - dotExitStart_ < activityDotExitTime;
    if (dotExiting) bounds = unite(bounds, grow(retiringDot_, 2, 2));
    const auto badge = privacyBadgeRect(now, compact);
    const auto count = std::ranges::count_if(captureIndicators_, &CaptureIndicator::shown);
    const bool interactiveBadge = !panelVisible && !splitVisible && !swapping && amount >= 1 &&
        count > 0 && badge.right - badge.left >= 18;
    const bool showPrivacyDetails = interactiveBadge && (privacyDetailsOpen_ || hovered(Hit::privacyBadge));
    const auto details = box((compact.left + compact.right) / 2 - 113, compact.bottom + 6,
                             226, 12 + 22.0f * static_cast<float>(count));
    if (showPrivacyDetails) bounds = unite(bounds, grow(details, 2, 2));
    bounds.top = std::max(bounds.top, 0.0f);
    origin_ = {static_cast<LONG>(std::floor(bounds.left * scale_)), static_cast<LONG>(std::floor(bounds.top * scale_))};
    const int width = static_cast<int>(std::ceil(bounds.right * scale_)) - origin_.x;
    const int height = static_cast<int>(std::ceil(bounds.bottom * scale_)) - origin_.y;
    prepareSurface(width, height, moving(now));
    const RECT pixels{0, 0, surfaceSize_.cx, surfaceSize_.cy};
    winrt::check_hresult(target_->BindDC(surface_, &pixels));

    base_ = D2D1::Matrix3x2F::Scale(scale_, scale_) *
            D2D1::Matrix3x2F::Translation(static_cast<float>(-origin_.x), static_cast<float>(-origin_.y));
    targets_.clear();
    scratchEditor_.reset();
    timerEditor_.reset();
    lyricViewport_ = {};
    dropViewport_ = {};
    appViewport_ = {};
    sourceMenuRect_ = {};
    audioOutputMenuRect_ = {};
    navHoverRect_ = {};
    bluetoothViewport_ = {};
    panelWheel_ = {};
    closingPanel_ = {};
    layerDepth_ = 0;
    target_->BeginDraw();
    target_->SetTransform(base_);
    target_->Clear(D2D1::ColorF(0, 0));
    if (splitVisible) {
        if (bubbleSlot && (!directClose || !closingDot_->idle))
            drawIsland(now, value, amount, morphTitle);
        if (directClose) {
            drawBubbleToDot(now, compact, fullBubble, slotBubble);
        } else if (growsFromDot) {
            drawBubbleFromDot(now, compact, fullBubble);
        } else {
            drawSplit(now, amount, bubbleSlot);
        }
    } else {
        if (handoff) drawActivityHandoff(now, compact);
        else if (drawSwap) drawActivitySwap(now, compact, swapEnd);
        else drawIsland(now, value, amount, morphTitle);
    }
    if (pendingHandoff && value < 0.18f) {
        pushLayer(nullptr, 1 - easeInOut(lin(0, 0.18f, value)));
        drawActivityDot(now, activitySwap_.dot, activitySwap_.incoming, false);
        popLayer();
    }
    if (interactiveBadge) addTarget(grow(badge, 1, 2), Hit::privacyBadge);
    if (showPrivacyDetails) drawPrivacyDetails(details);
    if (activityDot && !drawSwap) {
        if (slotOpacity < 1) pushLayer(nullptr, slotOpacity);
        drawActivityDot(now, *activityDot, companion, !splitVisible);
        if (slotOpacity < 1) popLayer();
    }
    if (dotExiting) {
        const float t = easeInOut(progress(dotExitStart_, activityDotExitTime, now));
        const auto& original = retiringDot_;
        const float middle = (original.top + original.bottom) / 2;
        const auto destination = box(original.left, middle, 0, 0);
        const auto shape = mixRect(original, destination, t);
        drawCompanionShell(shape, std::sin(pi * t) * 0.85f, 0.16f);
        pushLayer(nullptr, 1 - t);
        drawActivityIndicator(now, shape, retiringDotMode_, rgb(inkColor));
        popLayer();
    }
    const HRESULT result = target_->EndDraw();
    if (result == D2DERR_RECREATE_TARGET) {
        releaseDeviceResources();
        InvalidateRect(window, nullptr, FALSE);
        return;
    }
    winrt::check_hresult(result);

    POINT position{anchor_.x + origin_.x, anchor_.y + origin_.y};
    POINT source{};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, static_cast<BYTE>(std::lround(opacity * 255)), AC_SRC_ALPHA};
    winrt::check_bool(UpdateLayeredWindow(window, nullptr, &position, &surfaceSize_, surface_, &source,
                                         0, &blend, ULW_ALPHA));
}

}
