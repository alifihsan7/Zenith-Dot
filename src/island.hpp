#pragma once

#include "media.hpp"
#include "power.hpp"

#include <d2d1.h>
#include <dwrite.h>
#include <string_view>

namespace zenith {

enum class IslandMode { dot, media, power };

class Island {
public:
    Island() = default;
    ~Island();
    Island(Island const&) = delete;
    Island& operator=(Island const&) = delete;

    void setMode(IslandMode mode, bool animate);
    void draw(HWND window, MediaState const& media, PowerState const& power, bool animate,
              int hovered, int pressed);
    bool transitioning() const;
    bool animating() const;
    int buttonAt(HWND window, POINT point) const;

private:
    using Clock = std::chrono::steady_clock;

    float extent(Clock::time_point now) const;
    float themeProgress(Clock::time_point now) const;
    void updateTheme(CoverTheme const& theme, Clock::time_point now, bool animate);
    void prepareSurface(int width, int height);
    void prepareText();
    void drawMedia(MediaState const& media, Clock::time_point now, bool animate,
                   int hovered, int pressed, float opacity);
    void drawPower(PowerState const& power, float opacity);
    void text(std::wstring_view value, IDWriteTextFormat* format, D2D1_RECT_F rect, D2D1_COLOR_F color);
    ID2D1SolidColorBrush* ink(D2D1_COLOR_F color);

    winrt::com_ptr<ID2D1Factory> graphics_;
    winrt::com_ptr<ID2D1DCRenderTarget> target_;
    winrt::com_ptr<ID2D1SolidColorBrush> brush_;
    winrt::com_ptr<IDWriteFactory> fonts_;
    winrt::com_ptr<IDWriteTextFormat> titleFont_;
    winrt::com_ptr<IDWriteTextFormat> detailFont_;
    winrt::com_ptr<IDWriteInlineObject> ellipsis_;
    HDC surface_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ originalBitmap_ = nullptr;
    SIZE surfaceSize_{};
    IslandMode mode_ = IslandMode::dot;
    IslandMode content_ = IslandMode::dot;
    float startExtent_ = 0;
    Clock::time_point start_{};
    // Cover colours cross-fade from the *From_ values to theme_, starting at themeStart_.
    CoverTheme theme_;
    D2D1_COLOR_F accentFrom_ = D2D1::ColorF(CoverTheme{}.accent);
    D2D1_COLOR_F tintFrom_ = D2D1::ColorF(CoverTheme{}.tint);
    D2D1_COLOR_F accent_ = accentFrom_;
    Clock::time_point themeStart_{};
};

}
