#pragma once

#include <windows.h>
#include <oleidl.h>
#include <winrt/base.h>

#include <functional>
#include <cstdint>
#include <memory>
#include <string>
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

    ~DropZone();
    bool attach(HWND window, Hover hover, Changed changed);
    void detach();
    bool addImage(HBITMAP bitmap);
    void drag(size_t index);
    void openImage(size_t index, HWND owner) const;
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

    HWND window_ = nullptr;
    winrt::com_ptr<IDropTarget> target_;
    std::vector<Entry> items_;
    Changed changed_;
    std::wstring imageFolder_;
    std::vector<std::wstring> pendingDeletes_;
    unsigned imageNumber_ = 0;
    bool dragging_ = false;
};

}
