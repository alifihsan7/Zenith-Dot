#pragma once

#include <windows.h>
#include <oleidl.h>
#include <winrt/base.h>

#include <functional>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zenith {

struct DropThumbnail {
    unsigned width = 0;
    unsigned height = 0;
    std::vector<uint8_t> pixels;
};

struct DropPreview {
    std::wstring label;
    bool text = false;
    std::shared_ptr<const DropThumbnail> thumbnail;
};

class DropZone {
public:
    using Hover = std::function<void(bool)>;
    using Changed = std::function<void(std::vector<DropPreview>)>;
    enum class ImageResult { unavailable, duplicate, added };
    static constexpr UINT previewClosed = WM_APP + 11;

    ~DropZone();
    bool attach(HWND window, Hover hover, Changed changed);
    void detach();
    void setEnabled(bool enabled) { enabled_ = enabled; }
    ImageResult addImage(HBITMAP bitmap);
    bool canAddText(size_t length) const;
    bool addText(std::wstring_view text);
    void drag(size_t index);
    bool openImage(size_t index, HWND owner);
    HWND previewWindow() const { return previewWindow_; }
    void remove(size_t index);
    void clear();

private:
    struct Target;
    struct Entry {
        winrt::com_ptr<IDataObject> data;
        DropPreview preview;
        size_t bytes;
        std::wstring imagePath;
        bool temporary = false;
    };

    bool append(CLIPFORMAT type, HGLOBAL data);
    size_t remainingBytes() const;
    bool prepareImageFolder();
    void deleteTemporary(std::wstring const& path);
    void publish() const;
    static LRESULT CALLBACK previewProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

    HWND window_ = nullptr;
    winrt::com_ptr<IDropTarget> target_;
    std::vector<Entry> items_;
    Changed changed_;
    std::wstring imageFolder_;
    std::vector<std::wstring> pendingDeletes_;
    unsigned imageNumber_ = 0;
    std::optional<uint64_t> recentImageHash_;
    std::chrono::steady_clock::time_point recentImageAt_{};
    HWND previewWindow_ = nullptr;
    std::shared_ptr<const DropThumbnail> previewImage_;
    bool dragging_ = false;
    bool enabled_ = true;
};

}
