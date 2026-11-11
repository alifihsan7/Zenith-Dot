#include "visual_effects.hpp"

#include <dwmapi.h>

#include <algorithm>
#include <array>

namespace zenith {
namespace {

constexpr wchar_t backdropClass[] = L"ZenithDot.Backdrop";

LRESULT CALLBACK backdropProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCHITTEST) return HTTRANSPARENT;
    if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        BeginPaint(window, &paint);
        EndPaint(window, &paint);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

// Fractional box kernels avoid a visible step when the blur radius reaches zero.
void blurPass(std::uint32_t const* source, int sourceStride, std::uint32_t* destination, int destinationStride,
              int width, int height, float radius, bool horizontal) {
    const int extent = horizontal ? width : height;
    const int lines = horizontal ? height : width;
    const int sourceStep = horizontal ? 1 : sourceStride;
    const int destinationStep = horizontal ? 1 : destinationStride;
    const int whole = static_cast<int>(radius);
    const float fraction = radius - whole;
    const float divisor = 2 * whole + 1 + 2 * fraction;
    for (int line = 0; line < lines; ++line) {
        const auto* input = source + (horizontal ? line * sourceStride : line);
        auto* output = destination + (horizontal ? line * destinationStride : line);
        std::array<unsigned, 4> sum{};
        for (int offset = -whole; offset <= whole; ++offset) {
            const auto pixel = input[std::clamp(offset, 0, extent - 1) * sourceStep];
            for (unsigned channel = 0; channel < 4; ++channel) sum[channel] += (pixel >> (channel * 8)) & 255;
        }
        for (int position = 0; position < extent; ++position) {
            const auto before = input[std::max(0, position - whole - 1) * sourceStep];
            const auto after = input[std::min(extent - 1, position + whole + 1) * sourceStep];
            std::uint32_t blurred = 0;
            for (unsigned channel = 0; channel < 4; ++channel) {
                const auto edges = ((before >> (channel * 8)) & 255) + ((after >> (channel * 8)) & 255);
                const auto value = static_cast<std::uint32_t>((sum[channel] + fraction * edges) / divisor + 0.5f);
                blurred |= value << (channel * 8);
            }
            output[position * destinationStep] = blurred;
            const auto leaving = input[std::max(0, position - whole) * sourceStep];
            const auto entering = input[std::min(extent - 1, position + whole + 1) * sourceStep];
            for (unsigned channel = 0; channel < 4; ++channel) {
                sum[channel] -= (leaving >> (channel * 8)) & 255;
                sum[channel] += (entering >> (channel * 8)) & 255;
            }
        }
    }
}

}

VisualEffects::~VisualEffects() {
    if (backdrop_) DestroyWindow(backdrop_);
}

bool VisualEffects::createBackdrop() {
    WNDCLASSW type{};
    type.lpfnWndProc = backdropProc;
    type.hInstance = GetModuleHandleW(nullptr);
    type.lpszClassName = backdropClass;
    if (!RegisterClassW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    backdrop_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST, backdropClass, L"",
        WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, type.hInstance, nullptr);
    if (!backdrop_) return false;
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(backdrop_, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    const auto material = DWMSBT_TRANSIENTWINDOW;
    const MARGINS glass{-1, -1, -1, -1};
    if (FAILED(DwmSetWindowAttribute(backdrop_, DWMWA_SYSTEMBACKDROP_TYPE, &material, sizeof(material))) ||
        FAILED(DwmExtendFrameIntoClientArea(backdrop_, &glass))) {
        DestroyWindow(backdrop_);
        backdrop_ = nullptr;
        return false;
    }
    return true;
}

void VisualEffects::showBackdrop(HWND overlay, RECT bounds, int radius) {
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (width < 2 || height < 2) {
        hideBackdrop();
        return;
    }
    if (!backdropAvailable_) return;
    if (!backdrop_ && !createBackdrop()) {
        backdropAvailable_ = false;
        return;
    }
    if (EqualRect(&backdropBounds_, &bounds) && backdropRadius_ == radius && IsWindowVisible(backdrop_)) return;
    if (backdropSize_.cx != width || backdropSize_.cy != height || backdropRadius_ != radius) {
        const auto region = CreateRoundRectRgn(0, 0, width + 1, height + 1, radius * 2, radius * 2);
        if (!region || !SetWindowRgn(backdrop_, region, FALSE)) {
            if (region) DeleteObject(region);
            hideBackdrop();
            return;
        }
        backdropSize_ = {width, height};
        backdropRadius_ = radius;
    }
    if (SetWindowPos(backdrop_, overlay, bounds.left, bounds.top, width, height,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW)) backdropBounds_ = bounds;
}

void VisualEffects::hideBackdrop() {
    if (backdrop_ && IsWindowVisible(backdrop_)) ShowWindow(backdrop_, SW_HIDE);
}

void VisualEffects::blur(std::uint32_t* pixels, SIZE size, RECT bounds, float radius) {
    if (!pixels || radius <= 0.01f) {
        std::vector<std::uint32_t>{}.swap(blurBuffer_);
        return;
    }
    bounds.left = std::clamp(bounds.left, 0L, size.cx);
    bounds.right = std::clamp(bounds.right, bounds.left, size.cx);
    bounds.top = std::clamp(bounds.top, 0L, size.cy);
    bounds.bottom = std::clamp(bounds.bottom, bounds.top, size.cy);
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (!width || !height) return;
    radius = std::clamp(radius, 0.0f, 10.0f);
    blurBuffer_.resize(static_cast<size_t>(width) * height);
    auto* region = pixels + static_cast<size_t>(bounds.top) * size.cx + bounds.left;
    GdiFlush();
    blurPass(region, size.cx, blurBuffer_.data(), width, width, height, radius, true);
    blurPass(blurBuffer_.data(), width, region, size.cx, width, height, radius, false);
}

}
