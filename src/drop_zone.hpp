#pragma once

#include <windows.h>
#include <oleidl.h>
#include <winrt/base.h>

#include <functional>
#include <string>
#include <vector>

namespace zenith {

struct DropPreview {
    std::wstring label;
    bool text;
};

class DropZone {
public:
    using Hover = std::function<void(bool)>;
    using Changed = std::function<void(std::vector<DropPreview>)>;

    ~DropZone();
    bool attach(HWND window, Hover hover, Changed changed);
    void detach();
    void drag(size_t index);
    void remove(size_t index);
    void clear();

private:
    struct Target;
    struct Entry {
        winrt::com_ptr<IDataObject> data;
        DropPreview preview;
        size_t bytes;
    };

    bool append(CLIPFORMAT type, HGLOBAL data);
    void publish() const;

    HWND window_ = nullptr;
    winrt::com_ptr<IDropTarget> target_;
    std::vector<Entry> items_;
    Changed changed_;
    bool dragging_ = false;
};

}
