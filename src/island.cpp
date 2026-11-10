#include "island.hpp"

#include <algorithm>
#include <cmath>
#include <cwchar>

namespace zenith {
namespace {

constexpr float panelWidth = 360;
constexpr float panelHeight = 108;
constexpr float dotSize = 14;
constexpr float buttonY = 46;
constexpr float buttonX[] = {252, 289, 326};
constexpr auto animationTime = std::chrono::milliseconds(280);
constexpr auto themeTime = std::chrono::milliseconds(700);

D2D1_COLOR_F color(unsigned rgb, float opacity = 1.0f) {
    return D2D1::ColorF(rgb, opacity);
}

D2D1_COLOR_F mix(D2D1_COLOR_F const& from, D2D1_COLOR_F const& to, float t) {
    return D2D1::ColorF(std::lerp(from.r, to.r, t), std::lerp(from.g, to.g, t), std::lerp(from.b, to.b, t));
}

D2D1_COLOR_F faded(D2D1_COLOR_F value, float opacity) {
    value.a = opacity;
    return value;
}

std::wstring timestamp(double seconds) {
    const auto total = static_cast<long long>(seconds);
    const auto minutes = total / 60;
    const auto remainder = total % 60;
    return std::to_wstring(minutes) + (remainder < 10 ? L":0" : L":") + std::to_wstring(remainder);
}

}

Island::~Island() {
    if (originalBitmap_) SelectObject(surface_, originalBitmap_);
    if (bitmap_) DeleteObject(bitmap_);
    if (surface_) DeleteDC(surface_);
}

float Island::extent(Clock::time_point now) const {
    const float target = mode_ == IslandMode::dot ? 0.0f : 1.0f;
    if (now - start_ >= animationTime || startExtent_ == target) return target;
    const float t = std::chrono::duration<float>(now - start_).count() /
                    std::chrono::duration<float>(animationTime).count();
    const float spring = (1.0f - (1.0f + 8.0f * t) * std::exp(-8.0f * t)) / 0.99698084f;
    return std::lerp(startExtent_, target, spring);
}

void Island::setMode(IslandMode mode, bool animate) {
    if (mode == mode_) return;
    const auto now = Clock::now();
    startExtent_ = animate ? extent(now) : (mode == IslandMode::dot ? 0.0f : 1.0f);
    start_ = now;
    mode_ = mode;
    if (mode != IslandMode::dot) content_ = mode;
}

bool Island::transitioning() const {
    return extent(Clock::now()) != (mode_ == IslandMode::dot ? 0.0f : 1.0f);
}

bool Island::animating() const {
    return transitioning() || Clock::now() - themeStart_ < themeTime;
}

float Island::themeProgress(Clock::time_point now) const {
    if (now - themeStart_ >= themeTime) return 1;
    const float t = std::chrono::duration<float>(now - themeStart_).count() /
                    std::chrono::duration<float>(themeTime).count();
    return t * t * (3 - 2 * t);
}

void Island::updateTheme(CoverTheme const& theme, Clock::time_point now, bool animate) {
    if (theme == theme_) return;
    // Start from whatever is on screen now, so a track change mid-fade doesn't jump.
    const float progress = themeProgress(now);
    accentFrom_ = animate ? mix(accentFrom_, color(theme_.accent), progress) : color(theme.accent);
    tintFrom_ = animate ? mix(tintFrom_, color(theme_.tint), progress) : color(theme.tint);
    theme_ = theme;
    themeStart_ = animate ? now : Clock::time_point{};
}

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
    }
    if (!target_) {
        const auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
        winrt::check_hresult(graphics_->CreateDCRenderTarget(&properties, target_.put()));
        target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        winrt::check_hresult(target_->CreateSolidColorBrush(color(0xffffff), brush_.put()));
    }
}

void Island::prepareText() {
    if (ellipsis_) return;
    if (!fonts_) {
        winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(fonts_.put())));
    }
    const auto create = [this](auto& font, DWRITE_FONT_WEIGHT weight, float size) {
        if (font) return;
        winrt::check_hresult(fonts_->CreateTextFormat(L"Segoe UI", nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, size, L"", font.put()));
        font->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        font->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    };
    create(titleFont_, DWRITE_FONT_WEIGHT_SEMI_BOLD, 17);
    create(detailFont_, DWRITE_FONT_WEIGHT_NORMAL, 12);
    winrt::check_hresult(fonts_->CreateEllipsisTrimmingSign(detailFont_.get(), ellipsis_.put()));
    const DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
    titleFont_->SetTrimming(&trim, ellipsis_.get());
    detailFont_->SetTrimming(&trim, ellipsis_.get());
}

ID2D1SolidColorBrush* Island::ink(D2D1_COLOR_F value) {
    brush_->SetColor(value);
    return brush_.get();
}

void Island::text(std::wstring_view value, IDWriteTextFormat* format, D2D1_RECT_F rect, D2D1_COLOR_F tint) {
    if (!value.empty()) {
        target_->DrawText(value.data(), static_cast<UINT32>(value.size()), format, rect, ink(tint),
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
}

void Island::drawMedia(MediaState const& media, Clock::time_point now, bool animate,
                       int hovered, int pressed, float opacity) {
    const auto primary = color(0xf7f7f8, opacity);
    const auto secondary = color(0xc1c3ca, opacity);
    const auto accent = faded(accent_, opacity);
    const auto detail = media.artist.empty() ? media.album :
        (media.album.empty() ? media.artist : media.artist + L"  /  " + media.album);
    text(media.title.empty() ? L"Media playing" : media.title.c_str(), titleFont_.get(),
         D2D1::RectF(51, 22, 230, 47), primary);
    text(detail, detailFont_.get(), D2D1::RectF(51, 47, 230, 66), secondary);

    const double phase = animate ? std::chrono::duration<double>(now.time_since_epoch()).count() : 0;
    for (int bar = 0; bar < 4; ++bar) {
        const float x = 20.0f + bar * 6.0f;
        const float height = 7.0f + 13.0f * static_cast<float>(0.5 + 0.5 * std::sin(phase * 9 + bar * 1.7));
        const auto bounds = D2D1::RectF(x, 44 - height / 2, x + 3, 44 + height / 2);
        target_->FillRoundedRectangle(D2D1::RoundedRect(bounds, 1.5f, 1.5f), ink(accent));
    }

    for (int button = 0; button < 3; ++button) {
        const float x = buttonX[button];
        const auto circle = D2D1::Ellipse(D2D1::Point2F(x, buttonY), 16, 16);
        if (button == pressed || button == hovered) {
            target_->FillEllipse(circle, ink(color(button == pressed ? 0x51545b : 0x303237, opacity)));
        }
        auto* glyph = ink(primary);
        if (button == 1) {
            target_->FillRectangle(D2D1::RectF(x - 5, buttonY - 6, x - 1, buttonY + 6), glyph);
            target_->FillRectangle(D2D1::RectF(x + 1, buttonY - 6, x + 5, buttonY + 6), glyph);
        } else {
            const float direction = button == 0 ? -1.0f : 1.0f;
            const float edge = x + 5 * direction;
            target_->DrawLine(D2D1::Point2F(edge, buttonY - 6), D2D1::Point2F(edge, buttonY + 6), glyph, 2);
            target_->DrawLine(D2D1::Point2F(x - 5 * direction, buttonY - 5), D2D1::Point2F(edge - direction, buttonY),
                              glyph, 2);
            target_->DrawLine(D2D1::Point2F(x - 5 * direction, buttonY + 5), D2D1::Point2F(edge - direction, buttonY),
                              glyph, 2);
        }
    }

    if (media.duration > 0) {
        const auto position = media.positionAt(now);
        const float end = 20 + 320 * static_cast<float>(position / media.duration);
        target_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(20, 75, 340, 78), 1.5f, 1.5f),
                                      ink(color(0x62666e, opacity)));
        target_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(20, 75, end, 78), 1.5f, 1.5f), ink(accent));
        text(timestamp(position) + L" / " + timestamp(media.duration), detailFont_.get(),
             D2D1::RectF(20, 80, 340, 100), secondary);
    } else {
        text(L"Playback time unavailable", detailFont_.get(), D2D1::RectF(20, 77, 340, 100), secondary);
    }
}

void Island::drawPower(PowerState const& power, float opacity) {
    const auto primary = color(0xf7f7f8, opacity);
    const auto secondary = color(0xc1c3ca, opacity);
    const auto accent = faded(accent_, opacity);
    std::wstring title = L"Battery unavailable";
    std::wstring detail = L"Windows couldn't read battery status";
    if (power.available && !power.hasBattery) {
        title = L"No battery detected";
        detail = power.pluggedIn == true ? L"Connected to AC power" : L"Battery information unavailable";
    } else if (power.available) {
        title = power.percent ? std::to_wstring(*power.percent) + L"% battery" : L"Battery level unknown";
        detail = L"Power source unknown";
        if (power.pluggedIn == true) detail = power.charging ? L"Charging on AC power" : L"Plugged in";
        if (power.pluggedIn == false) detail = L"On battery";
        if (power.remaining) {
            const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(*power.remaining).count();
            detail += L"  /  " + std::to_wstring(minutes / 60) + L"h " +
                std::to_wstring(minutes % 60) + L"m remaining";
        }
    }
    text(title, titleFont_.get(), D2D1::RectF(20, 20, 340, 46), primary);
    text(detail, detailFont_.get(), D2D1::RectF(20, 47, 340, 70), secondary);
    if (power.percent) {
        const float end = 20 + 320 * static_cast<float>(*power.percent) / 100;
        target_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(20, 79, 340, 82), 1.5f, 1.5f),
                                      ink(color(0x62666e, opacity)));
        target_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(20, 79, end, 82), 1.5f, 1.5f), ink(accent));
    }
}

void Island::draw(HWND window, MediaState const& media, PowerState const& power, bool animate,
                  int hovered, int pressed) {
    const auto now = Clock::now();
    updateTheme(media.theme, now, animate);
    const float themed = themeProgress(now);
    accent_ = mix(accentFrom_, color(theme_.accent), themed);
    const auto tint = faded(mix(tintFrom_, color(theme_.tint), themed), 0.9f);
    const float openness = animate ? extent(now) : (mode_ == IslandMode::dot ? 0.0f : 1.0f);
    const bool compact = openness == 0 && mode_ == IslandMode::dot;
    const float width = compact ? dotSize + 2 : panelWidth + 2;
    const float height = compact ? dotSize + 2 : panelHeight + 2;
    const float dpiScale = GetDpiForWindow(window) / 96.0f;
    prepareSurface(static_cast<int>(std::ceil(width * dpiScale)), static_cast<int>(std::ceil(height * dpiScale)));
    const float opacity = std::clamp((openness - 0.88f) / 0.12f, 0.0f, 1.0f);
    if (opacity > 0) prepareText();
    const RECT pixels{0, 0, surfaceSize_.cx, surfaceSize_.cy};
    winrt::check_hresult(target_->BindDC(surface_, &pixels));

    const float bodyWidth = std::lerp(dotSize, panelWidth, openness);
    const float bodyHeight = std::lerp(dotSize, panelHeight, openness);
    const float left = (width - bodyWidth) / 2;
    const float radius = std::lerp(dotSize / 2, 26.0f, openness);
    const auto body = D2D1::RoundedRect(D2D1::RectF(left, 1, left + bodyWidth, 1 + bodyHeight), radius, radius);
    // Black-dominant shell: the cover only tints the far corner of the gradient.
    const D2D1_GRADIENT_STOP stops[]{{0, color(0, 0.92f)}, {1, tint}};
    winrt::com_ptr<ID2D1GradientStopCollection> gradient;
    winrt::check_hresult(target_->CreateGradientStopCollection(stops, 2, gradient.put()));
    winrt::com_ptr<ID2D1LinearGradientBrush> shell;
    winrt::check_hresult(target_->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties(D2D1::Point2F(left, 1), D2D1::Point2F(left + bodyWidth, 1 + bodyHeight)),
        gradient.get(), shell.put()));
    target_->BeginDraw();
    target_->SetTransform(D2D1::Matrix3x2F::Scale(D2D1::SizeF(dpiScale, dpiScale)));
    target_->Clear(color(0, 0));
    target_->FillRoundedRectangle(body, shell.get());
    target_->DrawRoundedRectangle(body, ink(color(0xffffff, 0.12f)), 1);
    if (opacity > 0) {
        if (content_ == IslandMode::media) drawMedia(media, now, animate && media.playing, hovered, pressed,
                                                    opacity);
        if (content_ == IslandMode::power) drawPower(power, opacity);
    }
    const HRESULT result = target_->EndDraw();
    if (result == D2DERR_RECREATE_TARGET) {
        brush_ = nullptr;
        target_ = nullptr;
        InvalidateRect(window, nullptr, FALSE);
        return;
    }
    winrt::check_hresult(result);

    const auto monitor = MonitorFromPoint(POINT{}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(MONITORINFO);
    winrt::check_bool(GetMonitorInfoW(monitor, &monitorInfo));
    const auto work = monitorInfo.rcWork;
    POINT position{work.left + (work.right - work.left - surfaceSize_.cx) / 2,
                   work.top + static_cast<LONG>(8 * dpiScale)};
    POINT origin{};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    winrt::check_bool(UpdateLayeredWindow(window, nullptr, &position, &surfaceSize_, surface_, &origin,
                                         0, &blend, ULW_ALPHA));
}

int Island::buttonAt(HWND window, POINT point) const {
    if (mode_ != IslandMode::media || transitioning()) return -1;
    const float scale = GetDpiForWindow(window) / 96.0f;
    for (int button = 0; button < 3; ++button) {
        if (std::abs(point.x / scale - buttonX[button]) <= 17 && std::abs(point.y / scale - buttonY) <= 18) {
            return button;
        }
    }
    return -1;
}

}
