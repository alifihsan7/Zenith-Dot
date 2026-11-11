#include "notifications.hpp"

#include <appmodel.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Notifications.h>

namespace zenith {

using namespace winrt::Windows::UI::Notifications;
using namespace winrt::Windows::UI::Notifications::Management;

bool hasPackageIdentity() {
    UINT32 size = 0;
    return GetCurrentPackageFullName(&size, nullptr) == ERROR_INSUFFICIENT_BUFFER;
}

winrt::Windows::Foundation::IAsyncAction requestNotificationAccess(HWND window) {
    const auto cancellation = co_await winrt::get_cancellation_token();
    cancellation.enable_propagation();
    auto access = NotificationAccess::Denied;
    try {
        access = co_await UserNotificationListener::Current().RequestAccessAsync();
    } catch (winrt::hresult_error const&) {
    }
    if (!cancellation()) PostMessageW(window, notificationAccessMessage, static_cast<WPARAM>(access), 0);
}

void Notifications::refresh(HWND window, bool enabled) {
    if (!enabled || !hasPackageIdentity()) {
        stop();
        return;
    }
    try {
        if (!listener_) listener_ = UserNotificationListener::Current();
        if (listener_.GetAccessStatus() != NotificationAccess::Allowed) {
            changed_.revoke();
            return;
        }
        if (!changed_) {
            changed_ = listener_.NotificationChanged(winrt::auto_revoke, [window](auto const&, auto const& event) {
                if (event.ChangeKind() == UserNotificationChangedKind::Added)
                    PostMessageW(window, notificationArrivedMessage, event.UserNotificationId(), 0);
            });
        }
    } catch (winrt::hresult_error const&) {
        stop();
    }
}

void Notifications::stop() {
    changed_.revoke();
    listener_ = nullptr;
}

std::optional<AppNotification> Notifications::read(unsigned id) const {
    if (!listener_ || !changed_) return std::nullopt;
    try {
        if (listener_.GetAccessStatus() != NotificationAccess::Allowed) return std::nullopt;
        const auto entry = listener_.GetNotification(id);
        if (!entry) return std::nullopt;
        const auto notification = entry.Notification();
        if (!notification) return std::nullopt;
        const auto visual = notification.Visual();
        if (!visual) return std::nullopt;
        const auto binding = visual.GetBinding(KnownNotificationBindings::ToastGeneric());
        if (!binding) return std::nullopt;
        const auto elements = binding.GetTextElements();
        const auto app = entry.AppInfo();
        const auto name = app ? app.DisplayInfo().DisplayName() : winrt::hstring{};
        std::wstring title = elements.Size() ? std::wstring(elements.GetAt(0).Text()) : std::wstring{};
        if (title.size() > 28) {
            title.resize(27);
            if (IS_HIGH_SURROGATE(title.back())) title.pop_back();
            title += L"…";
        }
        return AppNotification{name.empty() ? L"Notification" : std::wstring(name), std::move(title)};
    } catch (winrt::hresult_error const&) {
        return std::nullopt;
    }
}

}
