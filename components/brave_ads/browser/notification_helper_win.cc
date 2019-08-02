/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.ui.notifications.h>
#include <wrl/event.h>

#include "brave/components/brave_ads/browser/notification_helper_win.h"

#include "base/win/windows_version.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "base/win/scoped_winrt_initializer.h"
#include "base/feature_list.h"

namespace mswr = Microsoft::WRL;
namespace winui = ABI::Windows::UI;

namespace brave_ads {

NotificationHelperWin::NotificationHelperWin() = default;

NotificationHelperWin::~NotificationHelperWin() = default;

bool NotificationHelperWin::IsNotificationsAvailable() const {
  // There was a Microsoft bug in Windows 10 prior to build 17134 (i.e.
  // VERSION_WIN10_RS4) causing endless loops in displaying notifications. It
  // significantly amplified the memory and CPU usage. Therefore, Windows 10
  // native notifications are only enabled for build 17134 and later

  if (base::win::GetVersion() < base::win::VERSION_WIN10_RS4) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(features::kNativeNotifications)) {
    return false;
  }

  mswr::ComPtr<ABI::Windows::UI::Notifications::IToastNotifier> notifier_;

  if (!notifier_.Get() && FAILED(InitializeToastNotifier())) {
    return false;
  }

  winui::Notifications::IToastNotifier* notifier = notifier_.Get();

  winui::Notifications::NotificationSetting setting;

  HRESULT hr = notifier->get_Setting(&setting);
  if (!SUCCEEDED(hr)) {
    return false;
  }

  switch (setting) {
    case winui::Notifications::NotificationSetting_Enabled: {
      return true;
    }

    case winui::Notifications::NotificationSetting_DisabledForUser:
    case winui::Notifications::NotificationSetting_DisabledForApplication:
    case winui::Notifications::NotificationSetting_DisabledByGroupPolicy:
    case winui::Notifications::NotificationSetting_DisabledByManifest: {
      return false;
    }
  }

  return false;
}

HRESULT NotificationHelperWin::InitializeToastNotifier() {
  mswr::ComPtr<winui::Notifications::IToastNotificationManagerStatics> manager;

  HRESULT hr = CreateActivationFactory(
      RuntimeClass_Windows_UI_Notifications_ToastNotificationManager,
      manager.GetAddressOf());
  if (FAILED(hr)) {
    return hr;
  }

  ScopedHString application_id = ScopedHString::Create(GetAppId());
  hr = manager->CreateToastNotifierWithId(application_id.get(), &notifier_);
  if (FAILED(hr)) {
    return hr;
  }

  return hr;
}

bool NotificationHelperWin::ShowMyFirstAdNotification() const {
  return false;
}

NotificationHelperWin* NotificationHelperWin::GetInstance() {
  return base::Singleton<NotificationHelperWin>::get();
}

NotificationHelper* NotificationHelper::GetInstance() {
  return NotificationHelperWin::GetInstance();
}

}  // namespace brave_ads
