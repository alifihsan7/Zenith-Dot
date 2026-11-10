#include "drop_zone.hpp"

#include <ole2.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wincodec.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string_view>
#include <utility>

namespace zenith {
namespace {

constexpr size_t maxItems = 64;
constexpr size_t maxBytes = 16 * 1024 * 1024;
constexpr uint64_t maxImagePixels = 32'000'000;
constexpr auto imageDedupWindow = std::chrono::seconds(2);

FORMATETC format(CLIPFORMAT type) { return {type, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL}; }

struct Medium {
    STGMEDIUM value{};
    ~Medium() { ReleaseStgMedium(&value); }
};

struct Item : winrt::implements<Item, IDataObject> {
    Item(CLIPFORMAT type, HGLOBAL data) : type_(type), data_(data) {}
    ~Item() { GlobalFree(data_); }

    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* requested, STGMEDIUM* medium) noexcept override {
        if (!medium) return E_POINTER;
        *medium = {};
        const auto result = QueryGetData(requested);
        if (FAILED(result)) return result;
        const auto copy = OleDuplicateData(data_, type_, GMEM_MOVEABLE);
        if (!copy) return E_OUTOFMEMORY;
        medium->tymed = TYMED_HGLOBAL;
        medium->hGlobal = copy;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) noexcept override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* requested) noexcept override {
        if (!requested) return E_POINTER;
        if (requested->cfFormat != type_) return DV_E_FORMATETC;
        if (!(requested->tymed & TYMED_HGLOBAL)) return DV_E_TYMED;
        if (requested->dwAspect != DVASPECT_CONTENT) return DV_E_DVASPECT;
        return requested->lindex == -1 ? S_OK : DV_E_LINDEX;
    }
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC* output) noexcept override {
        if (!output) return E_POINTER;
        *output = {};
        return DATA_S_SAMEFORMATETC;
    }
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) noexcept override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD direction, IEnumFORMATETC** enumerator) noexcept override {
        if (direction != DATADIR_GET) return E_NOTIMPL;
        auto offered = format(type_);
        return SHCreateStdEnumFmtEtc(1, &offered, enumerator);
    }
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) noexcept override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) noexcept override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) noexcept override { return OLE_E_ADVISENOTSUPPORTED; }

private:
    CLIPFORMAT type_;
    HGLOBAL data_;
};

struct Source : winrt::implements<Source, IDropSource> {
    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape, DWORD keys) noexcept override {
        if (escape) return DRAGDROP_S_CANCEL;
        return (keys & MK_LBUTTON) ? S_OK : DRAGDROP_S_DROP;
    }
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) noexcept override { return DRAGDROP_S_USEDEFAULTCURSORS; }
};

CLIPFORMAT supported(IDataObject* data) {
    for (const auto type : {CF_HDROP, CF_UNICODETEXT}) {
        auto offered = format(static_cast<CLIPFORMAT>(type));
        if (data->QueryGetData(&offered) == S_OK) return offered.cfFormat;
    }
    return 0;
}

std::wstring textPreview(std::wstring_view contents) {
    const auto start = contents.find_first_not_of(L" \t\r\n");
    if (start == std::wstring_view::npos) return contents.empty() ? L"" : L"Text";
    const auto line = contents.substr(start);
    return std::wstring(line.substr(0, std::min(line.find_first_of(L"\r\n"), size_t{100})));
}

std::wstring textPreview(HGLOBAL data) {
    const auto raw = static_cast<wchar_t const*>(GlobalLock(data));
    if (!raw) return {};
    const auto label = textPreview(std::wstring_view(raw, wcsnlen(raw, GlobalSize(data) / sizeof(wchar_t))));
    GlobalUnlock(data);
    return label;
}

winrt::com_ptr<IDataObject> takeItem(CLIPFORMAT type, HGLOBAL data) {
    if (!data) winrt::throw_hresult(E_OUTOFMEMORY);
    try {
        return winrt::make<Item>(type, data);
    } catch (...) {
        GlobalFree(data);
        throw;
    }
}

winrt::com_ptr<IDataObject> fileItem(std::wstring const& path, size_t bytes) {
    const auto data = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
    if (!data) winrt::throw_hresult(E_OUTOFMEMORY);
    const auto header = static_cast<DROPFILES*>(GlobalLock(data));
    if (!header) {
        GlobalFree(data);
        winrt::throw_last_error();
    }
    header->pFiles = sizeof(DROPFILES);
    header->fWide = TRUE;
    std::memcpy(header + 1, path.c_str(), (path.size() + 1) * sizeof(wchar_t));
    GlobalUnlock(data);
    return takeItem(CF_HDROP, data);
}

std::optional<uint64_t> imageHash(IWICImagingFactory& factory, IWICBitmapSource& source) {
    UINT width = 0, height = 0;
    if (FAILED(source.GetSize(&width, &height)) || !width || !height ||
        static_cast<uint64_t>(width) * height > maxImagePixels) return std::nullopt;
    winrt::com_ptr<IWICFormatConverter> converter;
    if (FAILED(factory.CreateFormatConverter(converter.put())) ||
        FAILED(converter->Initialize(&source, GUID_WICPixelFormat24bppBGR, WICBitmapDitherTypeNone,
                                     nullptr, 0, WICBitmapPaletteTypeCustom))) return std::nullopt;
    const UINT stride = width * 3;
    const UINT rows = std::min(height, std::max(1u, 65536u / stride));
    std::vector<uint8_t> pixels(stride * rows);
    uint64_t hash = 14695981039346656037ull;
    constexpr uint64_t prime = 1099511628211ull;
    hash = (hash ^ width) * prime;
    hash = (hash ^ height) * prime;
    for (UINT y = 0; y < height; y += rows) {
        const UINT count = std::min(rows, height - y);
        const WICRect rect{0, static_cast<INT>(y), static_cast<INT>(width), static_cast<INT>(count)};
        const UINT bytes = stride * count;
        if (FAILED(converter->CopyPixels(&rect, stride, bytes, pixels.data()))) return std::nullopt;
        for (UINT i = 0; i < bytes; ++i) hash = (hash ^ pixels[i]) * prime;
    }
    return hash;
}

std::shared_ptr<DropThumbnail> makeThumbnail(IWICImagingFactory& factory, IWICBitmapSource& source,
                                             unsigned maxWidth, unsigned maxHeight) {
    UINT width = 0, height = 0;
    if (FAILED(source.GetSize(&width, &height)) || !width || !height ||
        static_cast<uint64_t>(width) * height > maxImagePixels) return {};
    const double scale = std::min({1.0, static_cast<double>(maxWidth) / width, static_cast<double>(maxHeight) / height});
    auto thumbnail = std::make_shared<DropThumbnail>();
    thumbnail->width = std::max(1u, static_cast<unsigned>(width * scale));
    thumbnail->height = std::max(1u, static_cast<unsigned>(height * scale));
    winrt::com_ptr<IWICBitmapScaler> scaler;
    winrt::com_ptr<IWICFormatConverter> converter;
    if (FAILED(factory.CreateBitmapScaler(scaler.put())) ||
        FAILED(scaler->Initialize(&source, thumbnail->width, thumbnail->height, WICBitmapInterpolationModeFant)) ||
        FAILED(factory.CreateFormatConverter(converter.put())) ||
        FAILED(converter->Initialize(scaler.get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                                     nullptr, 0, WICBitmapPaletteTypeCustom))) return {};
    const UINT stride = thumbnail->width * 4;
    const UINT bytes = stride * thumbnail->height;
    thumbnail->pixels.resize(bytes);
    if (FAILED(converter->CopyPixels(nullptr, stride, bytes, thumbnail->pixels.data()))) return {};
    return thumbnail;
}

std::shared_ptr<DropThumbnail> fileThumbnail(std::wstring const& path, unsigned maxWidth = 72, unsigned maxHeight = 54) {
    const auto dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return {};
    const auto* extension = path.c_str() + dot;
    constexpr wchar_t const* imageTypes[]{L".png", L".jpg", L".jpeg", L".bmp", L".gif", L".webp", L".tif", L".tiff"};
    if (std::ranges::none_of(imageTypes, [extension](auto type) { return _wcsicmp(extension, type) == 0; })) return {};
    winrt::com_ptr<IWICImagingFactory> factory;
    winrt::com_ptr<IWICBitmapDecoder> decoder;
    winrt::com_ptr<IWICBitmapFrameDecode> frame;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(factory.put()))) ||
        FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                  WICDecodeMetadataCacheOnDemand, decoder.put())) ||
        FAILED(decoder->GetFrame(0, frame.put()))) return {};
    return makeThumbnail(*factory, *frame, maxWidth, maxHeight);
}

bool encodePng(IWICImagingFactory& factory, IWICBitmapSource& source, std::wstring const& path) {
    winrt::com_ptr<IWICStream> stream;
    winrt::com_ptr<IWICBitmapEncoder> encoder;
    winrt::com_ptr<IWICBitmapFrameEncode> frame;
    winrt::com_ptr<IPropertyBag2> options;
    return SUCCEEDED(factory.CreateStream(stream.put())) &&
           SUCCEEDED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE)) &&
           SUCCEEDED(factory.CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.put())) &&
           SUCCEEDED(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache)) &&
           SUCCEEDED(encoder->CreateNewFrame(frame.put(), options.put())) &&
           SUCCEEDED(frame->Initialize(options.get())) &&
           SUCCEEDED(frame->WriteSource(&source, nullptr)) &&
           SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
}

}

struct DropZone::Target : winrt::implements<Target, IDropTarget> {
    Target(DropZone& owner, Hover hover) : owner_(owner), hover_(std::move(hover)) {}

    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* data, DWORD, POINTL, DWORD* effect) noexcept override {
        type_ = owner_.dragging_ ? 0 : supported(data);
        return update(effect);
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL, DWORD* effect) noexcept override { return update(effect); }
    HRESULT STDMETHODCALLTYPE DragLeave() noexcept override {
        type_ = 0;
        return highlight(false);
    }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* data, DWORD, POINTL, DWORD* effect) noexcept override {
        const auto allowed = *effect;
        *effect = DROPEFFECT_NONE;
        const auto type = std::exchange(type_, CLIPFORMAT{});
        try {
            if (const auto hr = highlight(false); FAILED(hr)) return hr;
            if (type == 0 || !(allowed & DROPEFFECT_COPY) || owner_.dragging_) return S_OK;
            auto offered = format(type);
            Medium medium;
            if (FAILED(data->GetData(&offered, &medium.value)) || medium.value.tymed != TYMED_HGLOBAL) return S_OK;
            if (owner_.append(type, medium.value.hGlobal)) *effect = DROPEFFECT_COPY;
            return S_OK;
        } catch (...) {
            return winrt::to_hresult();
        }
    }

private:
    HRESULT update(DWORD* effect) noexcept {
        const bool offered = type_ != 0 && (*effect & DROPEFFECT_COPY) != 0;
        *effect = offered && owner_.items_.size() < maxItems ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        return highlight(offered);
    }
    HRESULT highlight(bool value) noexcept {
        try {
            if (value != highlighted_) {
                highlighted_ = value;
                hover_(value);
            }
            return S_OK;
        } catch (...) {
            return winrt::to_hresult();
        }
    }

    DropZone& owner_;
    Hover hover_;
    CLIPFORMAT type_ = 0;
    bool highlighted_ = false;
};

DropZone::~DropZone() { detach(); }

bool DropZone::attach(HWND window, Hover hover, Changed changed) {
    changed_ = std::move(changed);
    auto target = winrt::make<Target>(*this, std::move(hover));
    if (FAILED(RegisterDragDrop(window, target.get()))) return false;
    window_ = window;
    target_ = std::move(target);
    return true;
}

void DropZone::detach() {
    if (previewWindow_) DestroyWindow(previewWindow_);
    if (window_) RevokeDragDrop(std::exchange(window_, nullptr));
    target_ = nullptr;
    for (const auto& item : items_) if (item.temporary) DeleteFileW(item.imagePath.c_str());
    items_.clear();
    for (const auto& path : pendingDeletes_) DeleteFileW(path.c_str());
    pendingDeletes_.clear();
    recentImageHash_.reset();
    if (!imageFolder_.empty()) RemoveDirectoryW(std::exchange(imageFolder_, {}).c_str());
}

size_t DropZone::remainingBytes() const {
    size_t remaining = maxBytes;
    for (const auto& item : items_) remaining -= item.bytes;
    return remaining;
}

bool DropZone::canAddText(size_t length) const {
    return length && items_.size() < maxItems && length < remainingBytes() / sizeof(wchar_t);
}

bool DropZone::addText(std::wstring_view text) {
    if (!canAddText(text.size())) return false;
    auto label = textPreview(text);
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    const auto data = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
    if (!data) return false;
    const auto destination = GlobalLock(data);
    if (!destination) {
        GlobalFree(data);
        return false;
    }
    std::memcpy(destination, text.data(), text.size() * sizeof(wchar_t));
    GlobalUnlock(data);
    items_.push_back({takeItem(CF_UNICODETEXT, data), {std::move(label), true, {}}, bytes, {}});
    publish();
    return true;
}

bool DropZone::prepareImageFolder() {
    if (!imageFolder_.empty()) return true;
    wchar_t temp[MAX_PATH + 1]{};
    const DWORD length = GetTempPathW(static_cast<DWORD>(std::size(temp)), temp);
    GUID guid{};
    wchar_t suffix[40]{};
    if (!length || length >= std::size(temp) || FAILED(CoCreateGuid(&guid)) ||
        !StringFromGUID2(guid, suffix, static_cast<int>(std::size(suffix)))) return false;
    auto folder = std::wstring(temp, length) + L"ZenithDot-" + suffix;
    if (!CreateDirectoryW(folder.c_str(), nullptr)) return false;
    imageFolder_ = std::move(folder);
    return true;
}

void DropZone::deleteTemporary(std::wstring const& path) {
    if (!path.empty() && !DeleteFileW(path.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND)
        pendingDeletes_.push_back(path);
}

DropZone::ImageResult DropZone::addImage(HBITMAP bitmap) {
    if (!bitmap) return ImageResult::unavailable;
    winrt::com_ptr<IWICImagingFactory> factory;
    winrt::com_ptr<IWICBitmap> image;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(factory.put()))) ||
        FAILED(factory->CreateBitmapFromHBITMAP(bitmap, nullptr, WICBitmapIgnoreAlpha, image.put()))) return ImageResult::unavailable;
    const auto hash = imageHash(*factory, *image);
    const auto now = std::chrono::steady_clock::now();
    if (hash && hash == recentImageHash_ && now - recentImageAt_ < imageDedupWindow) return ImageResult::duplicate;
    if (items_.size() >= maxItems || !remainingBytes()) return ImageResult::unavailable;
    auto thumbnail = makeThumbnail(*factory, *image, 72, 54);
    if (!thumbnail || !prepareImageFolder()) return ImageResult::unavailable;

    auto label = L"Capture " + std::to_wstring(++imageNumber_) + L".png";
    auto path = imageFolder_ + L"\\" + label;
    struct UncommittedFile {
        std::wstring const& path;
        bool keep = false;
        ~UncommittedFile() { if (!keep) DeleteFileW(path.c_str()); }
    } temporary{path};
    if (!encodePng(*factory, *image, path)) return ImageResult::unavailable;
    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes) || attributes.nFileSizeHigh)
        return ImageResult::unavailable;
    const size_t itemBytes = sizeof(DROPFILES) + (path.size() + 2) * sizeof(wchar_t);
    const size_t bytes = itemBytes + attributes.nFileSizeLow + thumbnail->pixels.size();
    if (bytes > remainingBytes()) return ImageResult::unavailable;
    items_.push_back({fileItem(path, itemBytes), {std::move(label), false, std::move(thumbnail)}, bytes, path, true});
    temporary.keep = true;
    recentImageHash_ = hash;
    recentImageAt_ = std::chrono::steady_clock::now();
    publish();
    return ImageResult::added;
}

bool DropZone::append(CLIPFORMAT type, HGLOBAL data) {
    size_t remaining = remainingBytes();
    const size_t size = GlobalSize(data);
    if (size == 0 || size > remaining || items_.size() >= maxItems) return false;

    std::vector<Entry> incoming;
    if (type == CF_HDROP) {
        if (size < sizeof(DROPFILES)) return false;
        const auto files = reinterpret_cast<HDROP>(data);
        const auto count = DragQueryFileW(files, 0xffffffff, nullptr, 0);
        if (count == 0 || count > maxItems - items_.size()) return false;
        incoming.reserve(count);
        for (UINT index = 0; index < count; ++index) {
            const auto length = DragQueryFileW(files, index, nullptr, 0);
            if (length == 0 || length >= size) return false;
            std::wstring path(length + 1, L'\0');
            if (DragQueryFileW(files, index, path.data(), length + 1) != length) return false;
            path.resize(length);
            const size_t bytes = sizeof(DROPFILES) + (path.size() + 2) * sizeof(wchar_t);
            if (bytes > remaining) return false;
            const auto slash = path.find_last_of(L"\\/");
            auto label = slash == std::wstring::npos ? path : path.substr(slash + 1);
            if (label.empty()) label = path;
            auto thumbnail = fileThumbnail(path);
            if (thumbnail && thumbnail->pixels.size() > remaining - bytes) thumbnail.reset();
            const auto entryBytes = bytes + (thumbnail ? thumbnail->pixels.size() : 0);
            auto item = fileItem(path, bytes);
            if (!thumbnail) path.clear();
            incoming.push_back({std::move(item), {std::move(label), false, std::move(thumbnail)}, entryBytes, std::move(path)});
            remaining -= entryBytes;
        }
    } else {
        auto label = textPreview(data);
        if (label.empty()) return false;
        auto item = takeItem(type, OleDuplicateData(data, type, GMEM_MOVEABLE));
        incoming.push_back({std::move(item), {std::move(label), true, {}}, size, {}});
    }
    items_.reserve(items_.size() + incoming.size());
    for (auto& item : incoming) items_.push_back(std::move(item));
    publish();
    return true;
}

void DropZone::publish() const {
    if (!changed_) return;
    std::vector<DropPreview> previews;
    previews.reserve(items_.size());
    for (const auto& item : items_) previews.push_back(item.preview);
    changed_(std::move(previews));
}

void DropZone::drag(size_t index) {
    if (index >= items_.size() || dragging_) return;
    auto source = winrt::make<Source>();
    const auto item = items_[index].data;
    DWORD effect{};
    dragging_ = true;
    DoDragDrop(item.get(), source.get(), DROPEFFECT_COPY, &effect);
    dragging_ = false;
}

void DropZone::remove(size_t index) {
    if (index >= items_.size()) return;
    if (items_[index].temporary) deleteTemporary(items_[index].imagePath);
    items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(index));
    publish();
}

void DropZone::clear() {
    for (const auto& item : items_) if (item.temporary) deleteTemporary(item.imagePath);
    items_.clear();
    publish();
}

bool DropZone::openImage(size_t index, HWND owner) {
    if (index >= items_.size() || items_[index].imagePath.empty()) return false;
    auto image = fileThumbnail(items_[index].imagePath, 1440, 1000);
    if (!image) return false;
    const auto title = items_[index].preview.label + L" · Zenith Dot";
    if (previewWindow_) {
        previewImage_ = std::move(image);
        SetWindowTextW(previewWindow_, title.c_str());
        InvalidateRect(previewWindow_, nullptr, FALSE);
        ShowWindow(previewWindow_, SW_RESTORE);
        SetForegroundWindow(previewWindow_);
        return true;
    }

    WNDCLASSW type{};
    type.style = CS_HREDRAW | CS_VREDRAW;
    type.lpfnWndProc = previewProc;
    type.hInstance = GetModuleHandleW(nullptr);
    type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    type.lpszClassName = L"ZenithDot.DropPreview";
    if (!RegisterClassW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    MONITORINFO monitor{sizeof(MONITORINFO)};
    if (!GetMonitorInfoW(MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST), &monitor)) return false;
    const LONG workWidth = monitor.rcWork.right - monitor.rcWork.left;
    const LONG workHeight = monitor.rcWork.bottom - monitor.rcWork.top;
    const LONG width = std::min(1000L, workWidth * 4 / 5);
    const LONG height = std::min(700L, workHeight * 4 / 5);
    previewImage_ = std::move(image);
    previewWindow_ = CreateWindowExW(0, type.lpszClassName, title.c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
        monitor.rcWork.left + (workWidth - width) / 2,
        monitor.rcWork.top + (workHeight - height) / 2,
        width, height, owner, nullptr, type.hInstance, this);
    if (!previewWindow_) {
        previewImage_.reset();
        return false;
    }
    ShowWindow(previewWindow_, SW_SHOW);
    SetForegroundWindow(previewWindow_);
    return true;
}

LRESULT CALLBACK DropZone::previewProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCCREATE) {
        const auto creation = reinterpret_cast<CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(creation->lpCreateParams));
    }
    auto* zone = reinterpret_cast<DropZone*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (zone) {
        if (message == WM_KEYDOWN && wparam == VK_ESCAPE) {
            DestroyWindow(window);
            return 0;
        }
        if (message == WM_ERASEBKGND) return 1;
        if (message == WM_PAINT) {
            PAINTSTRUCT paint{};
            const auto dc = BeginPaint(window, &paint);
            RECT client{};
            GetClientRect(window, &client);
            SetDCBrushColor(dc, RGB(15, 20, 28));
            FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
            if (const auto& image = zone->previewImage_; image && client.right > 0 && client.bottom > 0) {
                const auto scale = std::min({1.0, static_cast<double>(client.right) / image->width,
                                             static_cast<double>(client.bottom) / image->height});
                const int width = std::max(1, static_cast<int>(std::lround(image->width * scale)));
                const int height = std::max(1, static_cast<int>(std::lround(image->height * scale)));
                BITMAPINFO info{};
                info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                info.bmiHeader.biWidth = static_cast<LONG>(image->width);
                info.bmiHeader.biHeight = -static_cast<LONG>(image->height);
                info.bmiHeader.biPlanes = 1;
                info.bmiHeader.biBitCount = 32;
                info.bmiHeader.biCompression = BI_RGB;
                SetStretchBltMode(dc, HALFTONE);
                SetBrushOrgEx(dc, 0, 0, nullptr);
                StretchDIBits(dc, (client.right - width) / 2, (client.bottom - height) / 2, width, height,
                    0, 0, static_cast<int>(image->width), static_cast<int>(image->height), image->pixels.data(),
                    &info, DIB_RGB_COLORS, SRCCOPY);
            }
            EndPaint(window, &paint);
            return 0;
        }
        if (message == WM_NCDESTROY) {
            zone->previewWindow_ = nullptr;
            zone->previewImage_.reset();
            if (const auto owner = GetWindow(window, GW_OWNER)) PostMessageW(owner, previewClosed, 0, 0);
        }
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

}
