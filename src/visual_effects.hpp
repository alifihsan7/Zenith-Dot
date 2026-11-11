#pragma once

#include <windows.h>

#include <cstdint>
#include <vector>

namespace zenith {

class VisualEffects {
public:
    ~VisualEffects();
    VisualEffects() = default;
    VisualEffects(VisualEffects const&) = delete;
    VisualEffects& operator=(VisualEffects const&) = delete;

    void showBackdrop(HWND overlay, RECT bounds, int radius);
    void hideBackdrop();
    void blur(std::uint32_t* pixels, SIZE size, RECT bounds, float radius);

private:
    bool createBackdrop();
    HWND backdrop_ = nullptr;
    bool backdropAvailable_ = true;
    RECT backdropBounds_{};
    SIZE backdropSize_{};
    int backdropRadius_ = 0;
    std::vector<std::uint32_t> blurBuffer_;
};

}
