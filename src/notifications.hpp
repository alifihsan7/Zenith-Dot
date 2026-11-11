#pragma once

#include <windows.h>
#include <winrt/Windows.UI.Notifications.Management.h>

#include <optional>
#include <string>

namespace zenith {

using NotificationAccess = winrt::Windows::UI::Notifications::Management::UserNotificationListenerAccessStatus;

inline constexpr UINT notificationArrivedMessage = WM_APP + 19;
inline constexpr UINT notificationAccessMessage = WM_APP + 20;

bool hasPackageIdentity();
winrt::Windows::Foundation::IAsyncAction requestNotificationAccess(HWND window);

struct AppNotification {
    std::wstring app;
    std::wstring title;
};

class Notifications {
public:
    void refresh(HWND window, bool enabled);
    void stop();
    std::optional<AppNotification> read(unsigned id) const;

private:
    winrt::Windows::UI::Notifications::Management::UserNotificationListener listener_{nullptr};
    winrt::Windows::UI::Notifications::Management::UserNotificationListener::NotificationChanged_revoker changed_;
};

}
