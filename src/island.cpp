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
constexpr float pillCenterY = pillTop + pillHeight / 2;
constexpr float pillLeft = -pillWidth / 2;
constexpr float dotSize = 12;
constexpr float panelWidth = 369;
constexpr float mediaPanelHeight = 236;
constexpr float maxLyricsWidth = 338;
constexpr float panelRadius = 22;
constexpr float padTop = 13;
constexpr float padX = 14;
constexpr float padBottom = 8;
constexpr float innerWidth = panelWidth - 2 * padX;
constexpr float navHeight = 31;
constexpr float shadowReach = 30;
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
constexpr float splitWidth = pillWidth + releaseGap + maxBubble;

constexpr auto openDelay = 50ms;
constexpr auto closeDelay = 185ms;
constexpr auto parkDelay = 5s;
constexpr auto pillGrowTime = 280ms;
constexpr auto pillShrinkTime = 320ms;
constexpr auto panelTime = 220ms;
constexpr auto themeTime = 700ms;
constexpr auto alertOpenTime = 540ms;
constexpr auto alertCloseTime = 440ms;
constexpr auto inlineAlertTime = 2000ms;
constexpr auto visibilityTime = 200ms;
constexpr auto lyricScrollTime = 190ms;
constexpr int focusLength = 25 * 60;

constexpr unsigned inkColor = 0xf5f6f8;
constexpr unsigned mutedColor = 0xa4aab4;
constexpr unsigned readoutColor = 0xc2e0ff;

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

float panelEase(float t) {
    return bezier(0.22f, 0.78f, 0.23f, 1, clamp01(t));
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
void releasedPath(PathBuilder& path, float left, float bx, float q, float bubbleLeft, float bubbleWidth) {
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
    path.move(pt(start, top)).line(pt(bx, top)).arc(A, R, false, true)
        .curve(hA, hT, T).curve(flip(hT), flip(hA), flip(A))
        .arc(pt(bx, bottom), R, false, true).line(pt(start, bottom)).arc(pt(start, top), R, false, true).close();
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

std::wstring timestamp(double seconds) {
    const auto total = static_cast<long long>(std::max(0.0, seconds));
    const auto remainder = total % 60;
    return std::to_wstring(total / 60) + (remainder < 10 ? L":0" : L":") + std::to_wstring(remainder);
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
    if (!open(L"shell:AppsFolder\\" + appId)) {
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

void Island::setMedia(MediaState const& media) {
    const auto now = Clock::now();
    if (pressed_ == Hit::source && (pressedIndex_ >= media.sources.size() || pressedIndex_ >= media_.sources.size() ||
        media.sources[pressedIndex_].appId != media_.sources[pressedIndex_].appId ||
        media.sources[pressedIndex_].id != media_.sources[pressedIndex_].id)) pressed_ = Hit::none;
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
    media_ = media;
    if (media_.playing) lastActive_ = now;
    updatePill(now);
}

void Island::setPower(PowerState const& power) {
    power_ = power;
}

void Island::setSystem(SystemStats const& stats) {
    system_ = stats;
}

void Island::setLyrics(LyricsView const& lyrics) { lyrics_ = &lyrics; }

void Island::setScratchpad(ScratchpadView const& scratchpad) { scratchpad_ = &scratchpad; }

void Island::setScratchEditing(bool editing) { scratchEditing_ = editing; }

void Island::closePanel() { setOpen(false, Clock::now()); }

bool Island::panelPinned() const { return open_ && panelPinned_; }

std::optional<RECT> Island::scratchEditorRect() const {
    if (!open_ || screen_ != Screen::scratch || !scratchEditor_) return std::nullopt;
    const auto box = *scratchEditor_;
    return RECT{anchor_.x + std::lround(box.left * scale_), anchor_.y + std::lround(box.top * scale_),
                anchor_.x + std::lround(box.right * scale_), anchor_.y + std::lround(box.bottom * scale_)};
}

bool Island::wantsLyrics() const {
    return open_ && screen_ == Screen::media && media_.hasSession && lyricsOpen_;
}

float Island::pillAmount(Clock::time_point now) const {
    if (!animate_) return pillTarget_;
    return std::lerp(pillFrom_, pillTarget_, progress(pillStart_, pillLength_, now));
}

float Island::panelValue(Clock::time_point now) const {
    const float target = open_ ? 1.0f : 0.0f;
    if (!animate_) return target;
    return std::lerp(panelFrom_, target, panelEase(progress(panelStart_, panelTime, now)));
}

float Island::screenHeight(Screen screen) const {
    switch (screen) {
    case Screen::media: return media_.hasSession ? mediaPanelHeight : 146;
    case Screen::focus: return 183.0f;
    case Screen::scratch: return 216.0f;
    case Screen::system: return 169.5f;
    }
    return 146.0f;
}

float Island::panelHeight(Clock::time_point now) const {
    return std::lerp(heightFrom_, screenHeight(screen_), panelEase(progress(screenStart_, panelTime, now)));
}

float Island::panelWidthAt(Clock::time_point now) const {
    const float width = screen_ == Screen::media && media_.hasSession && lyricsOpen_ ? panelWidth + lyricsWidth_ : panelWidth;
    return animate_ ? std::lerp(widthFrom_, width, panelEase(progress(widthStart_, panelTime, now))) : width;
}

D2D1_RECT_F Island::panelRect(Clock::time_point now) const {
    const float width = panelWidthAt(now);
    return box(-width / 2, pillTop, width, panelHeight(now) + inlineInset());
}

int Island::focusRemaining(Clock::time_point now) const {
    if (!focusUntil_) return focusLeft_;
    const float left = std::chrono::duration<float>(*focusUntil_ - now).count();
    return std::max(0, static_cast<int>(std::ceil(left)));
}

bool Island::moving(Clock::time_point now) const {
    if (!animate_) return false;
    return now - pillStart_ < pillLength_ || now - panelStart_ < panelTime || now - screenStart_ < panelTime ||
           now - widthStart_ < panelTime || (wantsLyrics() && now - lyricScrollStart_ < lyricScrollTime) ||
           now - themeStart_ < themeTime ||
           (bubbleShowing() && now - alertStart_ < alertLength_);
}

std::wstring Island::title() const {
    if (!media_.title.empty()) return media_.title;
    const auto name = appName(media_.appId);
    return name.empty() ? L"Media playing" : name;
}

void Island::updatePill(Clock::time_point now) {
    auto mode = PillMode::dot;
    if (media_.hasSession && (media_.playing || open_ || now - lastActive_ < parkDelay)) {
        mode = PillMode::media;
    } else if (focusUntil_ || (alert_ && alert_->kind == IslandAlert::Kind::focusDone)) {
        mode = PillMode::focus;
    }
    pillMode_ = mode;
    if (mode != PillMode::dot) {
        pillContent_ = mode;
        pillTitle_ = mode == PillMode::media ? title() : L"Focus";
        pillPlaying_ = media_.playing;
    }
    if (bubbleShowing()) return;   // never reshape while a bubble is split off
    const float target = mode == PillMode::dot ? 0.0f : 1.0f;
    if (target != pillTarget_) {
        pillFrom_ = pillAmount(now);
        pillTarget_ = target;
        pillStart_ = now;
        pillLength_ = target > 0 ? Clock::duration(pillGrowTime) : Clock::duration(pillShrinkTime);
    }
}

void Island::updateTheme(Clock::time_point now) {
    const auto target = media_.hasSession ? media_.theme : CoverTheme{};
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
    alert_.reset();
    queued_.reset();
    alertHoldUntil_.reset();
    alertFrom_ = alertTo_ = 0;
    setOpen(false, now);
    hovered_ = Hit::none;
    pointerCancel();
}

void Island::setCaptureColor(std::optional<unsigned> color) {
    captureColor_ = color;
}

void Island::setOpen(bool open, Clock::time_point now) {
    openAt_.reset();
    closeAt_.reset();
    if (open_ == open || (open && (suppressed_ || bubbleShowing()))) return;
    const bool lyricsVisible = wantsLyrics();
    const float shown = panelValue(now);
    // The cover and title travel between the pill and the panel when both show the track.
    const bool trackVisible = media_.hasSession && screen_ == Screen::media;
    if (open) {
        widthStart_ = {};
        screenFrom_ = screen_;
        screenStart_ = {};
        heightFrom_ = screenHeight(screen_);
    }
    const bool pill = pillAmount(now) >= 1;
    shareCover_ = trackVisible && pillMode_ != PillMode::focus && (pillContent_ == PillMode::media || !pill);
    shareTitle_ = shareCover_ && pill && pillContent_ == PillMode::media;
    open_ = open;
    updateLyricVisibility(lyricsVisible, now);
    if (!open) panelPinned_ = false;
    keepHeight_ = 0;
    panelFrom_ = shown;
    panelStart_ = now;
    hovered_ = pressed_ = Hit::none;
    dragging_ = false;
    buttonWasDown_ = true;
    if (open) lastActive_ = now;
    updatePill(now);
}

void Island::showScreen(Screen screen, Clock::time_point now) {
    if (screen == screen_) return;
    const bool lyricsVisible = wantsLyrics();
    heightFrom_ = panelHeight(now);
    widthFrom_ = panelWidthAt(now);
    keepHeight_ = std::max(keepHeight_, heightFrom_);
    screenFrom_ = screen_;
    screen_ = screen;
    updateLyricVisibility(lyricsVisible, now);
    screenStart_ = animate_ ? now : Clock::time_point{};
    widthStart_ = screenStart_;
}

void Island::toggleFocus(Clock::time_point now) {
    if (focusUntil_) {
        focusLeft_ = focusRemaining(now);
        focusUntil_.reset();
    } else {
        focusUntil_ = now + std::chrono::seconds(focusLeft_ > 0 ? focusLeft_ : focusLength);
    }
    updatePill(now);
}

void Island::tick() {
    const auto now = Clock::now();
    if (open_ && !panelPinned_) {
        // Hover hysteresis: entering needs the real pill, but the open panel stays while the pointer
        // is within keepOpenMargin of it. An outside click closes an unpinned panel.
        POINT cursor{};
        GetCursorPos(&cursor);
        const D2D1_POINT_2F point{(cursor.x - anchor_.x) / scale_, (cursor.y - anchor_.y) / scale_};
        const auto panel = panelRect(now);
        if (inside(box(panel.left, panel.top, panel.right - panel.left, screenHeight(screen_) + inlineInset()), point)) {
            keepHeight_ = 0;
        }
        auto keep = panel;
        keep.bottom = std::max(keep.bottom, keep.top + keepHeight_);
        if (inside(grow(keep, keepOpenMargin, keepOpenMargin), point) || dragging_ || scratchEditing_) {
            closeAt_.reset();
        } else if (!closeAt_) {
            closeAt_ = now + closeDelay;
        }
        const int button = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
        const bool down = (GetAsyncKeyState(button) & 0x8000) != 0;
        if (down && !buttonWasDown_ && !inside(panel, point)) setOpen(false, now);
        buttonWasDown_ = down;
    }
    if (openAt_ && now >= *openAt_) setOpen(true, now);
    if (closeAt_ && now >= *closeAt_) setOpen(false, now);
    if (focusUntil_ && now >= *focusUntil_) {
        focusUntil_.reset();
        focusLeft_ = focusLength;
        notify({IslandAlert::Kind::focusDone, L"Focus done", L"Take a break", std::nullopt, 3, 2600ms});
    }
    if (alert_) {
        if (alertInline_) {
            if (now >= alertUntil_) finishAlert(now);
        } else if (alertTo_ > 0) {
            if (!alertHoldUntil_ && progress(alertStart_, alertLength_, now) >= 1) {
                alertHoldUntil_ = now + alert_->hold;
            }
            if (alertHoldUntil_ && now >= *alertHoldUntil_) {
                alertFrom_ = 1;
                alertTo_ = 0;
                alertStart_ = now;
                alertLength_ = alertCloseTime;
                alertHoldUntil_.reset();
            }
        } else if (progress(alertStart_, alertLength_, now) >= 1) {
            finishAlert(now);
        }
    }
    if (media_.playing || open_) lastActive_ = now;
    updatePill(now);
    updateTheme(now);
}

unsigned Island::frameInterval() const {
    const auto now = Clock::now();
    if (animate_ && now - visibilityStart_ < visibilityTime) return 10;
    if (suppressed_) return focusUntil_ ? 1000 : 0;
    if (moving(now) || openAt_ || closeAt_ || dragging_) return 10;
    if (panelPinned() && !alert_) {
        if (screen_ == Screen::media && (media_.playing || (wantsLyrics() && now < lyricFollowAt_))) return 50;
        return screen_ == Screen::system || scratchEditing_ || focusUntil_ ? 1000 : 0;
    }
    if (open_ || alert_) return 50;
    if (pillMode_ == PillMode::media && media_.playing && animate_) return 33;
    if (pillMode_ != PillMode::dot) return 250;
    return 0;
}

bool Island::wantsSystemStats() const {
    return open_ && screen_ == Screen::system;
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

bool Island::pointerMove(POINT client) {
    const auto point = stagePoint(client);
    if (dragging_) {
        dragFraction_ = timelineFraction(point);
        return true;
    }
    const auto target = targetAt(point);
    const auto hit = target ? target->hit : Hit::none;
    const auto index = target ? target->index : 0;
    if (!open_) {
        if (hit == Hit::pill) {
            if (!openAt_) openAt_ = Clock::now() + openDelay;
        } else {
            openAt_.reset();
        }
    }
    if (hit == hovered_ && index == hoveredIndex_) return openAt_.has_value();
    hovered_ = hit;
    hoveredIndex_ = index;
    return true;
}

bool Island::pointerLeave() {
    openAt_.reset();
    if (hovered_ == Hit::none) return false;
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

bool Island::scrollLyrics(POINT client, int delta) {
    if (!wantsLyrics() || !inside(lyricViewport_, stagePoint(client))) return false;
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
    if (target->hit == Hit::lyricLine) {
        lyricScrollFrom_ = lyricScrollTo_ = lyricScrollShown_;
        lyricScrollStart_ = {};
    }
    pressed_ = target->hit;
    pressedIndex_ = target->index;
    return true;
}

IslandCommand Island::pointerUp(POINT client) {
    const auto now = Clock::now();
    IslandCommand command;
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
    if (!target || target->hit != hit || target->index != index) return command;
    switch (hit) {
    case Hit::previous: command.kind = IslandCommand::Kind::previous; break;
    case Hit::toggle: command.kind = IslandCommand::Kind::toggle; break;
    case Hit::next: command.kind = IslandCommand::Kind::next; break;
    case Hit::source:
        if (index < media_.sources.size()) {
            command.kind = IslandCommand::Kind::selectSource;
            const auto& source = media_.sources[index];
            command.appId = source.id.empty() ? source.appId : source.id;
        }
        break;
    case Hit::navMedia: showScreen(Screen::media, now); break;
    case Hit::navFocus: showScreen(Screen::focus, now); break;
    case Hit::navScratch: showScreen(Screen::scratch, now); break;
    case Hit::navSystem: showScreen(Screen::system, now); break;
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
        command.seconds = lyrics_->lines[index].seconds;
        lyricFollowAt_ = now + 500ms;
        break;
    case Hit::retryLyrics: command.kind = IslandCommand::Kind::retryLyrics; break;
    case Hit::scratchEdit: command.kind = IslandCommand::Kind::editScratchpad; break;
    case Hit::panelPin:
        panelPinned_ = !panelPinned_;
        closeAt_.reset();
        break;
    case Hit::focusToggle: toggleFocus(now); break;
    case Hit::focusReset:
        focusUntil_.reset();
        focusLeft_ = focusLength;
        updatePill(now);
        break;
    default: break;
    }
    return command;
}

void Island::pointerCancel() {
    pressed_ = Hit::none;
    dragging_ = false;
}

bool Island::overControl() const {
    return hovered_ != Hit::none;
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

void Island::prepareSurface(int width, int height) {
    if (!surface_) {
        surface_ = CreateCompatibleDC(nullptr);
        if (!surface_) winrt::throw_last_error();
    }
    if (surfaceSize_.cx != width || surfaceSize_.cy != height) {
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
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
        surfaceSize_ = {width, height};
    }
    if (!graphics_) {
        winrt::check_hresult(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, graphics_.put()));
        const auto round = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                                                       D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND);
        winrt::check_hresult(graphics_->CreateStrokeStyle(round, nullptr, 0, roundStroke_.put()));
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
    create(timerFont_, L"Segoe UI", 12, strong, end);
    create(titleFont_, L"Segoe UI", 16, strong, start);
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

void Island::drawChipGlyph(D2D1_POINT_2F center, D2D1_COLOR_F color) {
    target_->DrawRoundedRectangle(rounded(box(center.x - 5, center.y - 5, 10, 10), 2), ink(color), 1.2f);
    for (const float offset : {-2.5f, 2.5f}) {
        target_->DrawLine({center.x + offset, center.y - 8}, {center.x + offset, center.y - 6}, ink(color), 1.2f);
        target_->DrawLine({center.x + offset, center.y + 6}, {center.x + offset, center.y + 8}, ink(color), 1.2f);
        target_->DrawLine({center.x - 8, center.y + offset}, {center.x - 6, center.y + offset}, ink(color), 1.2f);
        target_->DrawLine({center.x + 6, center.y + offset}, {center.x + 8, center.y + offset}, ink(color), 1.2f);
    }
}

void Island::drawButton(D2D1_RECT_F rect, std::wstring_view label, Hit hit, bool record) {
    const float fill = pressed(hit) ? 0.14f : hovered(hit) ? 0.08f : 0.04f;
    target_->FillRoundedRectangle(rounded(rect, 8), ink(white(fill)));
    target_->DrawRoundedRectangle(rounded(grow(rect, -0.5f, -0.5f), 7.5f), ink(white(0.12f)), 1);
    text(label, buttonFont_.get(), rect, rgb(inkColor));
    if (record) addTarget(rect, hit);
}

// The cover, or the app's logo on a dark tile when the player gives no cover.
void Island::drawArt(D2D1_RECT_F rect, float radius) {
    const float size = rect.right - rect.left;
    if (auto* image = bitmap(cover_)) {
        winrt::com_ptr<ID2D1BitmapBrush> fill;
        winrt::check_hresult(target_->CreateBitmapBrush(image, fill.put()));
        fill->SetInterpolationMode(D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        fill->SetTransform(D2D1::Matrix3x2F::Scale(size / cover_.width, (rect.bottom - rect.top) / cover_.height) *
                           D2D1::Matrix3x2F::Translation(rect.left, rect.top));
        target_->FillRoundedRectangle(rounded(rect, radius), fill.get());
        return;
    }
    target_->FillRoundedRectangle(rounded(rect, radius), ink(white(0.08f)));
    const D2D1_POINT_2F center{(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2};
    if (auto* logo = icon(media_.appId); logo && bitmap(*logo)) {
        const float side = size * 0.55f;
        target_->DrawBitmap(logo->bitmap.get(), box(center.x - side / 2, center.y - side / 2, side, side), 1,
                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    } else if (size >= 24) {
        text(L"♫", glyphFont_.get(), rect, rgb(mutedColor));
    }
}

// ---- Compact: dot and pill -----------------------------------------------------------------------

static D2D1_RECT_F compactShape(float amount) {
    const float t = easeInOut(clamp01(amount));
    const float width = std::lerp(dotSize, pillWidth, t);
    const float height = std::lerp(dotSize, pillHeight, t);
    return box(-width / 2, pillCenterY - height / 2, width, height);
}

void Island::drawCompact(Clock::time_point now, float amount, bool hideShared) {
    if (bubbleShowing()) {
        drawSplit(now, amount);
        return;
    }
    // A nearly transparent hit area: layered windows only receive the pointer over painted pixels.
    const auto hit = amount >= 1 ? box(pillLeft, pillTop - 3, pillWidth, 46) : box(-22, pillTop - 3, 44, 46);
    target_->FillRectangle(hit, ink(D2D1::ColorF(0, 0, 0, 1.0f / 255)));
    addTarget(hit, Hit::pill);

    const auto shape = compactShape(amount);
    const float radius = std::min(shape.right - shape.left, shape.bottom - shape.top) / 2;
    fillShell(rounded(shape, radius));
    if (captureColor_ && amount < 1) {
        target_->FillRoundedRectangle(rounded(shape, radius), ink(rgb(*captureColor_, 1 - amount)));
    }
    target_->DrawRoundedRectangle(rounded(grow(shape, -0.5f, -0.5f), radius - 0.5f), ink(white(0.13f)), 1);

    const float opacity = lin(0.55f, 1, amount);
    if (opacity <= 0) return;
    winrt::com_ptr<ID2D1RoundedRectangleGeometry> clip;
    winrt::check_hresult(graphics_->CreateRoundedRectangleGeometry(rounded(shape, radius), clip.put()));
    pushLayer(clip.get(), opacity);
    drawPillContent(now, hideShared);
    popLayer();
}

void Island::drawPillContent(Clock::time_point now, bool hideShared) {
    const float edge = pillLeft + pillWidth - 12;
    const float right = edge - (captureColor_ ? 11 : 0);
    if (captureColor_) {
        target_->FillEllipse(D2D1::Ellipse({edge, pillCenterY}, 3, 3), ink(rgb(*captureColor_)));
    }
    if (pillContent_ == PillMode::focus) {
        drawFocusGlyph({pillLeft + 19, pillCenterY}, 14, accent_);
        const bool done = !focusUntil_ && alert_ && alert_->kind == IslandAlert::Kind::focusDone;
        text(done ? L"0:00" : timestamp(focusRemaining(now)), timerFont_.get(), box(right - 40, pillTop, 40, pillHeight),
             accent_);
        text(pillTitle_, pillTitleFont_.get(), box(pillLeft + 34, pillTop, right - 48 - (pillLeft + 34), pillHeight),
             rgb(inkColor));
        return;
    }
    if (!hideShared) target_->FillEllipse(D2D1::Ellipse({pillLeft + 16, pillCenterY}, 4, 4), ink(accent_));
    float titleEnd = right;
    if (pillPlaying_) {
        // Four bars, each scaling between 40% and 100% (V7's 0.75s alternate ease-in-out loop).
        constexpr float heights[]{8, 14, 10, 6};
        constexpr float delays[]{0, -0.39f, -0.24f, -0.55f};
        const float left = right - 17.5f;
        const double seconds = std::chrono::duration<double>(now.time_since_epoch()).count();
        for (int bar = 0; bar < 4; ++bar) {
            float scale = 1;
            if (animate_) {
                const double phase = (seconds - delays[bar]) / 0.75;
                float t = static_cast<float>(phase - std::floor(phase));
                if (static_cast<long long>(std::floor(phase)) % 2 != 0) t = 1 - t;
                scale = 0.4f + 0.6f * bezier(0.42f, 0, 0.58f, 1, t);
            }
            const float height = heights[bar] * scale;
            target_->FillRoundedRectangle(
                rounded(box(left + bar * 5, pillCenterY - height / 2, 2.5f, height), 1.25f), ink(accent_));
        }
        titleEnd = left - 8;
    } else {
        // Paused: the panel's pause glyph (two bars), scaled for the pill.
        const float left = right - 10;
        for (const float x : {left, left + 5.5f}) {
            target_->FillRoundedRectangle(rounded(box(x, pillCenterY - 5, 2.5f, 10), 1), ink(rgb(mutedColor)));
        }
        titleEnd = left - 8;
    }
    if (!hideShared || !shareTitle_) {
        text(pillTitle_, pillTitleFont_.get(), box(pillLeft + 28, pillTop, titleEnd - (pillLeft + 28), pillHeight),
             rgb(inkColor));
    }
}

// ---- Panel ---------------------------------------------------------------------------------------

void Island::drawPanel(Clock::time_point now, float value, float amount) {
    const auto full = panelRect(now);
    const auto from = compactShape(amount);
    const float fromRadius = std::min(from.right - from.left, from.bottom - from.top) / 2;
    const auto clip = mixRect(from, full, value);
    const float radius = std::lerp(fromRadius, panelRadius, value);
    const float opacity = open_ ? std::lerp(0.68f, 1.0f, value) : value;

    // Soft drop shadow (0 8px 22px at 35%), faded in as the panel settles.
    const float shadow = lin(0.6f, 1, value) * opacity;
    for (int ring = 0; ring < 8; ++ring) {
        const float spread = -2.0f + ring * 3.0f;
        const auto rect = grow(box(clip.left, clip.top + 8, clip.right - clip.left, clip.bottom - clip.top),
                               spread, spread);
        target_->FillRoundedRectangle(rounded(rect, radius + spread), ink(D2D1::ColorF(0, 0, 0, 0.045f * shadow)));
    }

    winrt::com_ptr<ID2D1RoundedRectangleGeometry> mask;
    winrt::check_hresult(graphics_->CreateRoundedRectangleGeometry(rounded(clip, radius), mask.put()));
    pushLayer(mask.get(), opacity);
    fillShell(rounded(clip, radius));
    target_->DrawRoundedRectangle(rounded(grow(clip, -0.5f, -0.5f), radius - 0.5f), ink(white(0.12f)), 1);

    recordPanel_ = open_ && value >= 1;
    const bool hideShared = shareCover_ && value < 1;
    auto content = full;
    if (const float inset = inlineInset(); inset > 0) {
        drawInlineAlert(full);
        content.top += inset;
    }
    const float switching = progress(screenStart_, panelTime, now);
    if (switching < 1 && screenFrom_ != screen_) {
        // Cross-fade: the old screen fades out fast, the new one fades in slightly later and rises 3px.
        const float ms = elapsedMs(screenStart_, now);
        pushLayer(nullptr, 1 - easeOut(clamp01(ms / 120)));
        drawScreen(screenFrom_, content, now, false, false);
        popLayer();
        const float enter = easeOut(clamp01((ms - 48) / 195));
        if (enter > 0) {
            target_->SetTransform(D2D1::Matrix3x2F::Translation(0, 3 * (1 - enter)) * base_);
            pushLayer(nullptr, enter);
            drawScreen(screen_, content, now, false, recordPanel_);
            popLayer();
            target_->SetTransform(base_);
        }
    } else {
        drawScreen(screen_, content, now, hideShared, recordPanel_);
    }
    drawNav(full, recordPanel_);
    popLayer();

    if (shareCover_ && value < 1 && value > 0) drawShared(content, value, amount);
}

void Island::drawScreen(Screen screen, D2D1_RECT_F panel, Clock::time_point now, bool hideShared, bool record) {
    switch (screen) {
    case Screen::media: drawMedia(panel, now, hideShared, record); break;
    case Screen::focus: drawFocus(panel, now, record); break;
    case Screen::scratch: drawScratchpad(panel, record); break;
    case Screen::system: drawSystem(panel); break;
    }
}

void Island::drawMedia(D2D1_RECT_F panel, Clock::time_point now, bool hideShared, bool record) {
    const float x = panel.left;
    const float y = panel.top;
    if (!media_.hasSession) {
        text(L"Not Playing", headingCenterFont_.get(), box(x + padX, y + padTop, innerWidth, 88), rgb(mutedColor));
        return;
    }
    // Track: cover, title, artist.
    if (!hideShared) drawArt(box(x + 14, y + 13, 44, 44), 8);
    if (!hideShared || !shareTitle_) text(title(), titleFont_.get(), box(x + 68, y + 15.4f, 247, 22.4f), rgb(inkColor));
    const auto& detail = media_.artist.empty() ? media_.album : media_.artist;
    text(detail, bodyFont_.get(), box(x + 68, y + 37.8f, 247, 16.8f), rgb(mutedColor));

    const auto lyricButton = box(x + 321, y + 17, 34, 34);
    if (lyricsOpen_ || hovered(Hit::lyrics) || pressed(Hit::lyrics)) {
        target_->FillRoundedRectangle(rounded(lyricButton, 7), ink(white(0.10f)));
    }
    const auto glyph = lyricsOpen_ ? rgb(readoutColor) : rgb(inkColor);
    const float gx = lyricButton.left + 9, gy = lyricButton.top + 9;
    PathBuilder speech(graphics_.get());
    speech.move(pt(gx + 1, gy + 2), false).line(pt(gx + 15, gy + 2)).line(pt(gx + 15, gy + 11))
        .line(pt(gx + 8, gy + 11)).line(pt(gx + 4, gy + 14)).line(pt(gx + 4, gy + 11))
        .line(pt(gx + 1, gy + 11)).close();
    target_->DrawGeometry(speech.finish().get(), ink(glyph), 1.4f, roundStroke_.get());
    target_->DrawLine(pt(gx + 4, gy + 5), pt(gx + 12, gy + 5), ink(glyph), 1.4f, roundStroke_.get());
    target_->DrawLine(pt(gx + 4, gy + 8), pt(gx + 9, gy + 8), ink(glyph), 1.4f, roundStroke_.get());
    if (record) addTarget(lyricButton, Hit::lyrics);

    // Timeline.
    const auto rail = box(x + 14, y + 69, innerWidth, 15);
    timelineRect_ = rail;
    const float middle = rail.top + 7.5f;
    const bool timed = media_.duration > 0;
    const double position = media_.positionAt(now);
    const float fraction = dragging_ ? dragFraction_ : timed ? static_cast<float>(position / media_.duration) : 0;
    target_->FillRoundedRectangle(rounded(box(rail.left, middle - 2, innerWidth, 4), 2), ink(white(0.18f)));
    if (timed) {
        const float end = rail.left + innerWidth * fraction;
        target_->FillRoundedRectangle(rounded(D2D1::RectF(rail.left, middle - 2, end, middle + 2), 2), ink(accent_));
        if (media_.canSeek) {
            const float knob = dragging_ || hovered(Hit::timeline) ? 7.0f : 6.0f;
            target_->FillEllipse(D2D1::Ellipse({end, middle}, knob, knob), ink(accent_));
            if (record) addTarget(grow(rail, 0, 4), Hit::timeline);
        }
        const double shown = dragging_ ? fraction * media_.duration : position;
        text(timestamp(shown), monoFont_.get(), box(x + 14, y + 84, 170, 14), rgb(mutedColor));
        text(timestamp(media_.duration), monoEndFont_.get(), box(x + 185, y + 84, 170, 14), rgb(mutedColor));
    } else {
        text(L"Live", monoFont_.get(), box(x + 14, y + 84, 170, 14), rgb(mutedColor));
    }

    // Transport: previous, play/pause on an accent disc, next.
    const float middleY = y + 122;
    const float centerX = x + panelWidth / 2;
    const Hit buttons[]{Hit::previous, Hit::toggle, Hit::next};
    const auto onAccent = mixColor(accent_, D2D1::ColorF(0, 0, 0), 0.86f);
    for (int i = 0; i < 3; ++i) {
        const bool enabled = buttons[i] == Hit::toggle || media_.canSkip;
        const D2D1_POINT_2F c{centerX + (i - 1) * 45.0f, middleY};
        const auto disc = D2D1::Ellipse(c, 18, 18);
        if (buttons[i] == Hit::toggle) {
            auto face = accent_;
            if (pressed(Hit::toggle)) face = mixColor(face, D2D1::ColorF(0, 0, 0), 0.18f);
            else if (hovered(Hit::toggle)) face = mixColor(face, D2D1::ColorF(1, 1, 1), 0.12f);
            target_->FillEllipse(disc, ink(face));
            if (media_.playing) {
                target_->FillRectangle(box(c.x - 5, c.y - 6, 3.5f, 12), ink(onAccent));
                target_->FillRectangle(box(c.x + 1.5f, c.y - 6, 3.5f, 12), ink(onAccent));
            } else {
                fillPolygon({{c.x - 4, c.y - 6}, {c.x + 6, c.y}, {c.x - 4, c.y + 6}}, onAccent);
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
        if (record && enabled) addTarget(box(c.x - 18, c.y - 18, 36, 36), buttons[i]);
    }

    // Sources: one round button per app with a media session, marked by its logo.
    target_->FillRectangle(box(x + padX, y + 148, innerWidth, 1), ink(white(0.12f)));
    for (size_t i = 0; i < media_.sources.size() && i < 10; ++i) {
        const auto& source = media_.sources[i];
        const auto rect = box(x + 14 + static_cast<float>(i) * 34, y + 155, 30, 30);
        const D2D1_POINT_2F c{rect.left + 15, rect.top + 15};
        if (pressed(Hit::source, i)) target_->FillEllipse(D2D1::Ellipse(c, 15, 15), ink(white(0.14f)));
        else if (hovered(Hit::source, i)) target_->FillEllipse(D2D1::Ellipse(c, 15, 15), ink(white(0.08f)));
        const bool selected = source.id.empty() ? media_.sourceId.empty() && source.appId == media_.appId :
                                                 source.id == media_.sourceId;
        if (selected) target_->DrawEllipse(D2D1::Ellipse(c, 14.5f, 14.5f), ink(accent_), 1);
        if (auto* logo = icon(source.appId); logo && bitmap(*logo)) {
            target_->DrawBitmap(logo->bitmap.get(), box(c.x - 9, c.y - 9, 18, 18), 1,
                                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        } else {
            target_->FillEllipse(D2D1::Ellipse(c, 9, 9), ink(white(0.16f)));
            const auto name = appName(source.appId);
            text(name.substr(0, 1), letterFont_.get(), box(c.x - 9, c.y - 9, 18, 18), rgb(inkColor));
        }
        if (source.tabNumber > 0) {
            const auto badge = pt(c.x + 10, c.y + 10);
            target_->FillEllipse(D2D1::Ellipse(badge, 7, 7), ink(rgb(0x253347)));
            text(std::to_wstring(source.tabNumber), letterFont_.get(), box(badge.x - 7, badge.y - 7, 14, 14),
                 rgb(inkColor));
        }
        if (record) addTarget(rect, Hit::source, i);
    }
    if (panel.right - panel.left > panelWidth) {
        drawLyrics(panel, now, dragging_ ? dragFraction_ * media_.duration : position);
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
    const float x = panel.left + panelWidth + 8, y = panel.top + 16;
    const float width = lyricsWidth_ - padX - 8;
    constexpr float viewportHeight = mediaPanelHeight - 16 - 34 - padX;
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
    } else if (synced && media_.canSeek && width >= 170) {
        text(L"Click a line to seek", bodyEndFont_.get(), box(x + 57, y, width - 57, 22), rgb(mutedColor));
    }
    const auto& lines = lyrics_->lines;
    const auto next = std::ranges::upper_bound(lines, position, {}, &LyricLine::seconds);
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
    lyricViewport_ = box(x, y + 34, width, viewportHeight);
    lyricScrollShown_ = std::clamp(lyricScroll(now), 0.0f, std::max(0.0f, lyricContentHeight_ - viewportHeight));
    target_->PushAxisAlignedClip(lyricViewport_, D2D1_ANTIALIAS_MODE_ALIASED);
    target_->DrawTextLayout(pt(x, lyricViewport_.top - lyricScrollShown_), lyricLayout_.get(),
                            ink(rgb(plain ? inkColor : mutedColor)));
    target_->PopAxisAlignedClip();
}

void Island::drawFocus(D2D1_RECT_F panel, Clock::time_point now, bool record) {
    const float x = panel.left;
    const float y = panel.top;
    text(L"Focus", titleFont_.get(), box(x + padX, y + padTop, innerWidth, 22.4f), rgb(inkColor));
    const int left = focusRemaining(now);
    text(timestamp(left), clockFont_.get(), box(x + padX, y + 43.4f, innerWidth, 44.8f), rgb(inkColor));
    const auto label = focusUntil_ ? L"Pause" : (left == focusLength || left == 0) ? L"Start" : L"Resume";
    const float centerX = x + panelWidth / 2;
    drawButton(box(centerX - 68, y + 96.2f, 64, 30), label, Hit::focusToggle, record);
    drawButton(box(centerX + 4, y + 96.2f, 64, 30), L"Reset", Hit::focusReset, record);
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
        const bool empty = !scratchpad_ || scratchpad_->text.empty();
        text(empty ? L"Write a quick note…" : std::wstring_view(scratchpad_->text), scratchFont_.get(),
             grow(editor, -7, -4), rgb(empty ? mutedColor : inkColor));
    }
    if (record) {
        scratchEditor_ = editor;
        addTarget(field, Hit::scratchEdit);
    }
}

void Island::drawSystem(D2D1_RECT_F panel) {
    const float x = panel.left;
    const float y = panel.top;
    text(L"System", titleFont_.get(), box(x + padX, y + padTop, innerWidth, 22.4f), rgb(inkColor));
    const std::pair<wchar_t const*, unsigned> rows[]{{L"CPU", system_.cpu}, {L"RAM", system_.memory}};
    float top = y + 44.4f;
    for (const auto& [label, value] : rows) {
        text(label, bodyFont_.get(), box(x + 14, top, 37, 17), rgb(inkColor));
        const auto bar = box(x + 59, top + 5.5f, 248, 6);
        target_->FillRoundedRectangle(rounded(bar, 3), ink(white(0.12f)));
        target_->FillRoundedRectangle(rounded(D2D1::RectF(bar.left, bar.top, bar.left + 248 * std::min(value, 100u) / 100.0f,
                                                          bar.bottom), 3), ink(accent_));
        text(std::to_wstring(value) + L"%", bodyEndFont_.get(), box(x + 315, top, 40, 17), rgb(inkColor));
        top += 26;
    }
    std::wstring status = L"Power status unavailable";
    if (power_.available && !power_.hasBattery) {
        status = L"No battery";
    } else if (power_.available) {
        status = power_.pluggedIn == true ? (power_.charging ? L"Charging" : L"Plugged in") :
                 power_.pluggedIn == false ? L"On Battery" : L"Battery";
    }
    if (power_.remaining) {
        const auto minutes = std::chrono::ceil<std::chrono::minutes>(*power_.remaining).count();
        status += L" · ";
        if (minutes >= 60) status += std::to_wstring(minutes / 60) + L"h ";
        status += std::to_wstring(minutes % 60) + L"m left";
    }
    const auto level = power_.percent ? std::to_wstring(*power_.percent) + L"%" : std::wstring{};
    text(status, bodyFont_.get(), box(x + 14, y + 96.4f, 260, 17), rgb(readoutColor));
    text(level, strongEndFont_.get(), box(x + 275, y + 96.4f, 80, 17), rgb(readoutColor));
}

void Island::drawNav(D2D1_RECT_F panel, bool record) {
    const float top = panel.bottom - padBottom - navHeight;
    target_->FillRectangle(box(panel.left + padX, top - 6, innerWidth, 1), ink(white(0.12f)));
    const float start = panel.left + panelWidth / 2 - (4 * 35 + 3 * 8) / 2.0f;
    const Hit hits[]{Hit::navMedia, Hit::navFocus, Hit::navScratch, Hit::navSystem};
    const Screen screens[]{Screen::media, Screen::focus, Screen::scratch, Screen::system};
    for (int i = 0; i < 4; ++i) {
        const auto rect = box(start + i * 43.0f, top, 35, navHeight);
        const bool selected = screen_ == screens[i];
        if (selected) target_->FillRoundedRectangle(rounded(rect, 7), ink(withAlpha(accent_, 0.14f)));
        else if (hovered(hits[i])) target_->FillRoundedRectangle(rounded(rect, 7), ink(white(0.08f)));
        const auto color = selected ? accent_ : rgb(mutedColor);
        const D2D1_POINT_2F c{rect.left + 17.5f, rect.top + 15.5f};
        if (i == 0) text(L"♫", glyphFont_.get(), rect, color);
        if (i == 1) drawFocusGlyph(c, 15, color);
        if (i == 2) text(L"✎", glyphFont_.get(), rect, color);
        if (i == 3) drawChipGlyph(c, color);
        if (record) addTarget(rect, hits[i]);
    }
    const auto pin = box(panel.left + panelWidth - padX - 34, top + 1, 34, 29);
    drawButton(pin, L"", Hit::panelPin, record);
    const bool pinned = panelPinned();
    const auto color = pinned ? accent_ : rgb(inkColor);
    const float x = pin.left + 17, y = pin.top + 14;
    PathBuilder glyph(graphics_.get());
    glyph.move(pt(x - 4.5f, y - 7), pinned).line(pt(x + 4.5f, y - 7)).line(pt(x + 2.5f, y - 4))
        .line(pt(x + 2.5f, y - 1)).line(pt(x + 5, y + 2)).line(pt(x - 5, y + 2))
        .line(pt(x - 2.5f, y - 1)).line(pt(x - 2.5f, y - 4)).close();
    const auto shape = glyph.finish();
    if (pinned) target_->FillGeometry(shape.get(), ink(color));
    else target_->DrawGeometry(shape.get(), ink(color), 1.4f, roundStroke_.get());
    target_->DrawLine(pt(x, y + 2), pt(x, y + 7), ink(color), 1.4f, roundStroke_.get());
}

// The cover (and title) travel between their pill and panel spots while the panel opens or closes.
void Island::drawShared(D2D1_RECT_F panel, float value, float amount) {
    const bool pill = amount >= 1 && pillContent_ == PillMode::media;
    const auto fromCover = pill ? box(pillLeft + 12, pillCenterY - 4, 8, 8) : compactShape(amount);
    const auto toCover = box(panel.left + 14, panel.top + 13, 44, 44);
    const float fromRadius = (fromCover.right - fromCover.left) / 2;
    drawArt(mixRect(fromCover, toCover, value), std::lerp(fromRadius, 8.0f, value));
    if (!shareTitle_) return;
    // The title is drawn at its panel size and scaled down to the pill's 12px at the start.
    const float scale = std::lerp(12.0f / 16.0f, 1.0f, value);
    const float x = std::lerp(pillLeft + 28, panel.left + 68, value);
    const float y = std::lerp(pillCenterY - 22.4f * 0.75f / 2, panel.top + 15.4f, value);
    const float width = std::lerp(82.0f / 0.75f, 247.0f, value);
    target_->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) * D2D1::Matrix3x2F::Translation(x, y) * base_);
    text(title(), titleFont_.get(), box(0, 0, width, 22.4f), rgb(inkColor));
    target_->SetTransform(base_);
}

// ---- Alerts --------------------------------------------------------------------------------------

void Island::notify(IslandAlert alert) {
    if (suppressed_) return;
    const auto now = Clock::now();
    prepareText();
    if (alert_ && alert_->kind == alert.kind && (alert.kind == IslandAlert::Kind::copy || alert.meter)) {
        if (alertInline_) {
            alertUntil_ = now + inlineAlertTime;
        } else if (alertTo_ == 0) {
            alertFrom_ = alertValue(now);
            alertTo_ = 1;
            alertStart_ = now;
            alertLength_ = std::chrono::duration_cast<Clock::duration>(alertOpenTime * (1 - alertFrom_));
        } else if (alertHoldUntil_) {
            alertHoldUntil_ = now + alert.hold;
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

void Island::startAlert(IslandAlert alert, Clock::time_point now, bool update) {
    float width = 12 + 15 + 12;
    if (!alert.title.empty()) width += 7 + measure(alert.title, alertTitleFont_.get());
    if (!alert.value.empty()) width += 7 + measure(alert.value, alertValueFont_.get());
    alertWidth_ = alert.meter ? 168.0f : std::clamp(width + 1, 52.0f, maxBubble);
    alert_ = std::move(alert);
    if (update) return;
    // A dot <-> pill morph in flight snaps to its end so the split starts from a settled shape.
    pillFrom_ = pillTarget_;
    pillStart_ = {};
    alertHoldUntil_.reset();
    alertInline_ = open_;
    if (alertInline_) {
        alertUntil_ = now + inlineAlertTime;
        return;
    }
    openAt_.reset();
    alertFrom_ = 0;
    alertTo_ = 1;
    alertStart_ = now;
    alertLength_ = alertOpenTime;
}

void Island::finishAlert(Clock::time_point now) {
    alert_.reset();
    alertFrom_ = alertTo_ = 0;
    alertHoldUntil_.reset();
    updatePill(now);
    if (queued_) {
        auto next = std::move(*queued_);
        queued_.reset();
        startAlert(std::move(next), now, false);
    }
}

float Island::alertValue(Clock::time_point now) const {
    if (!animate_) return alertTo_;
    return std::lerp(alertFrom_, alertTo_, progress(alertStart_, alertLength_, now));
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

void Island::drawSplit(Clock::time_point now, float amount) {
    const float p = alertValue(now);
    const float target = alertWidth_;
    PathBuilder shape(graphics_.get());
    float left = pillLeft;
    D2D1_RECT_F area{};
    float alertOpacity = 0;
    float copyOpacity = 0;
    const bool fromPill = amount >= 1;
    if (fromPill) {
        // Stretch until the pinch, then widen. The whole shape stays centered and only grows.
        const float travel = easeInOut(lin(0, pinchAt, p));
        const float widen = easeInOut(lin(pinchAt, 1, p));
        const float d = (2 * capRadius + releaseGap) * travel;
        const float bubbleWidth = std::lerp(pillHeight, target, widen);
        const float span = p < pinchAt ? pillWidth + d : pillWidth + releaseGap + bubbleWidth;
        left = -span / 2;
        const float bx = left + pillWidth - capRadius;
        alertOpacity = lin(pinchAt - 0.1f, pinchAt + 0.12f, p);
        copyOpacity = lin(0.8f, 1, p);
        if (p < pinchAt) {
            // The bubble is a full-size copy of the pill's cap sliding out of it.
            const float k = 1 - std::pow(lin(0.28f, pinchAt, p), 2.0f);
            area = box(bx + d - capRadius, pillTop, 2 * capRadius, pillHeight);
            if (d < 0.5f) capsule(shape, box(left, pillTop, pillWidth, pillHeight));
            else connectedPath(shape, left, bx, d, capRadius, k);
        } else {
            const float q = easeOut(lin(pinchAt, pinchAt + 0.28f, p));
            area = box(left + pillWidth + releaseGap, pillTop, bubbleWidth, pillHeight);
            releasedPath(shape, left, bx, q, area.left, bubbleWidth);
        }
    } else {
        // From the idle dot the bubble grows out of the dot itself.
        const float width = std::lerp(dotSize, target, p);
        const float height = std::lerp(dotSize, pillHeight, p);
        area = box(-width / 2, pillCenterY - height / 2, width, height);
        capsule(shape, area);
        const float t = lin(0.31f, 0.82f, p);
        alertOpacity = t * t * (3 - 2 * t);
        copyOpacity = 1;
    }
    const auto geometry = shape.finish();
    fillShell(geometry.get());
    if (!fromPill && captureColor_) {
        target_->FillGeometry(geometry.get(), ink(rgb(*captureColor_, 1 - easeOut(p))));
    }
    target_->DrawGeometry(geometry.get(), ink(white(0.13f)), 1);

    if (fromPill) {
        winrt::com_ptr<ID2D1RoundedRectangleGeometry> clip;
        winrt::check_hresult(graphics_->CreateRoundedRectangleGeometry(
            rounded(box(left, pillTop, pillWidth, pillHeight), capRadius), clip.put()));
        pushLayer(clip.get(), 1);
        target_->SetTransform(D2D1::Matrix3x2F::Translation(left - pillLeft, 0) * base_);
        drawPillContent(now, false);
        target_->SetTransform(base_);
        popLayer();
    }
    drawAlertContent(area, alertOpacity, copyOpacity);
}

void Island::drawAlertContent(D2D1_RECT_F area, float opacity, float copyOpacity) {
    if (opacity <= 0 || !alert_) return;
    const float width = area.right - area.left;
    const float radius = std::min(width, area.bottom - area.top) / 2;
    winrt::com_ptr<ID2D1RoundedRectangleGeometry> clip;
    winrt::check_hresult(graphics_->CreateRoundedRectangleGeometry(rounded(area, radius), clip.put()));
    pushLayer(clip.get(), opacity);
    const float middle = (area.top + area.bottom) / 2;
    // While the bubble is narrow the icon sits centered; the text fades in as it reaches full width.
    float x = area.left + std::lerp(std::max(0.0f, (width - 15) / 2), 12.0f, copyOpacity);
    drawAlertIcon(alert_->kind, pt(x, middle - 7.5f), alert_->color ? rgb(*alert_->color) : accent_);
    x += 15 + 7;
    const float end = area.right - 12;
    if (!alert_->title.empty() && end > x) {
        const float titleWidth = measure(alert_->title, alertTitleFont_.get());
        text(alert_->title, alertTitleFont_.get(), box(x, middle - 10, std::min(titleWidth + 1, end - x), 20),
             withAlpha(rgb(inkColor), copyOpacity));
        x += titleWidth + 7;
    }
    if (alert_->meter) {
        drawAlertMeter(x, end, middle, copyOpacity);
    } else if (!alert_->value.empty() && end > x) {
        text(alert_->value, alertValueFont_.get(), box(x, middle - 10, end - x, 20),
             withAlpha(rgb(mutedColor), copyOpacity));
    }
    popLayer();
}

// The V7 alert glyphs, drawn in a 16-unit box scaled to 15 DIPs.
void Island::drawAlertIcon(IslandAlert::Kind kind, D2D1_POINT_2F origin, D2D1_COLOR_F color) {
    const float scale = 15.0f / 16;
    target_->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) * D2D1::Matrix3x2F::Translation(origin.x, origin.y) *
                          base_);
    switch (kind) {
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
        drawFocusGlyph(pt(8, 8), 16, color);
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
    case IslandAlert::Kind::volume: {
        fillPolygon({pt(1.5f, 6), pt(4, 6), pt(7.5f, 3), pt(7.5f, 13), pt(4, 10), pt(1.5f, 10)}, color);
        if (alert_->value == L"Muted") {
            target_->DrawLine(pt(10.5f, 6), pt(14.5f, 10), ink(color), 1.5f, roundStroke_.get());
            target_->DrawLine(pt(14.5f, 6), pt(10.5f, 10), ink(color), 1.5f, roundStroke_.get());
        } else {
            PathBuilder waves(graphics_.get());
            waves.move(pt(10, 5.5f), false).arc(pt(10, 10.5f), 3.5f, false, true);
            waves.move(pt(12.5f, 3.5f), false).arc(pt(12.5f, 12.5f), 6.5f, false, true);
            target_->DrawGeometry(waves.finish().get(), ink(color), 1.5f, roundStroke_.get());
        }
        break;
    }
    case IslandAlert::Kind::brightness:
        target_->DrawEllipse(D2D1::Ellipse(pt(8, 8), 3, 3), ink(color), 1.4f);
        for (int ray = 0; ray < 8; ++ray) {
            const float angle = ray * 0.78539816f;
            const float x = std::cos(angle), y = std::sin(angle);
            target_->DrawLine(pt(8 + 5 * x, 8 + 5 * y), pt(8 + 6.5f * x, 8 + 6.5f * y),
                              ink(color), 1.4f, roundStroke_.get());
        }
        break;
    }
    target_->SetTransform(base_);
}

void Island::drawAlertMeter(float left, float right, float middle, float opacity) {
    const float labelWidth = measure(alert_->value, alertValueFont_.get()) + 1;
    const float width = right - left - labelWidth - 8;
    if (width <= 0) return;
    target_->FillRoundedRectangle(rounded(box(left, middle - 2, width, 4), 2), ink(white(0.16f * opacity)));
    const float fill = width * *alert_->meter / 100;
    if (fill > 0) {
        target_->FillRoundedRectangle(rounded(box(left, middle - 2, fill, 4), 2), ink(withAlpha(accent_, opacity)));
    }
    text(alert_->value, alertValueFont_.get(), box(right - labelWidth, middle - 10, labelWidth, 20),
         withAlpha(rgb(mutedColor), opacity));
}

// While the panel is open an alert shows as a row at its top instead of a bubble.
void Island::drawInlineAlert(D2D1_RECT_F panel) {
    const float y = panel.top + padTop;
    drawAlertIcon(alert_->kind, pt(panel.left + padX, y), alert_->color ? rgb(*alert_->color) : accent_);
    if (alert_->meter) {
        drawAlertMeter(panel.left + padX + 21, panel.right - padX, y + 7.5f, 1);
        return;
    }
    auto line = alert_->title;
    if (!line.empty() && !alert_->value.empty()) line += L" ";
    line += alert_->value;
    text(line, alertValueFont_.get(), box(panel.left + padX + 21, y - 2, innerWidth - 21, 19), rgb(inkColor));
}

// ---- Frame ---------------------------------------------------------------------------------------

void Island::draw(HWND window) {
    const auto now = Clock::now();
    updateTheme(now);
    scale_ = GetDpiForWindow(window) / 96.0f;
    const auto monitor = MonitorFromPoint(POINT{}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(MONITORINFO);
    winrt::check_bool(GetMonitorInfoW(monitor, &monitorInfo));
    const auto work = monitorInfo.rcWork;
    lyricsWidth_ = std::clamp((work.right - work.left) / scale_ - panelWidth - 2 * shadowReach,
                              130.0f, maxLyricsWidth);
    const float opacity = visibility(now);
    anchor_ = {work.left + (work.right - work.left) / 2, work.top - std::lround((1 - opacity) * 14 * scale_)};

    const float amount = pillAmount(now);
    const float value = panelValue(now);
    const bool panelVisible = open_ || value > 0;
    const bool compactVisible = !open_;
    if (!panelVisible || panelWidthAt(now) <= panelWidth) lyricLayout_ = nullptr;

    // The surface only covers what is drawn this frame.
    auto bounds = grow(compactShape(amount), 2, 2);
    bounds = unite(bounds, amount >= 1 ? box(pillLeft, pillTop - 3, pillWidth, 46) : box(-22, pillTop - 3, 44, 46));
    if (panelVisible) bounds = unite(bounds, grow(panelRect(now), shadowReach, shadowReach));
    if (bubbleShowing()) bounds = unite(bounds, box(-splitWidth / 2 - 2, pillTop - 2, splitWidth + 4, pillHeight + 4));
    bounds.top = std::max(bounds.top, 0.0f);
    origin_ = {static_cast<LONG>(std::floor(bounds.left * scale_)), static_cast<LONG>(std::floor(bounds.top * scale_))};
    const int width = static_cast<int>(std::ceil(bounds.right * scale_)) - origin_.x;
    const int height = static_cast<int>(std::ceil(bounds.bottom * scale_)) - origin_.y;
    prepareSurface(width, height);
    prepareText();
    const RECT pixels{0, 0, surfaceSize_.cx, surfaceSize_.cy};
    winrt::check_hresult(target_->BindDC(surface_, &pixels));

    base_ = D2D1::Matrix3x2F::Scale(scale_, scale_) *
            D2D1::Matrix3x2F::Translation(static_cast<float>(-origin_.x), static_cast<float>(-origin_.y));
    targets_.clear();
    scratchEditor_.reset();
    lyricViewport_ = {};
    layerDepth_ = 0;
    target_->BeginDraw();
    target_->SetTransform(base_);
    target_->Clear(D2D1::ColorF(0, 0));
    if (compactVisible) drawCompact(now, amount, shareCover_ && value > 0);
    if (panelVisible) drawPanel(now, value, amount);
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
