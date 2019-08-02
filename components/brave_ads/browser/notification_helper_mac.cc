/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/browser/notification_helper_mac.h"

namespace brave_ads {

NotificationHelperMac::NotificationHelperMac() = default;

NotificationHelperMac::~NotificationHelperMac() = default;

bool NotificationHelperMac::IsNotificationsAvailable() const {
  // macOS 10.13 and older do not support native notifications. Chromium will
  // fall back to the Chromium Message Center if native notifications are not
  // supported

  // Until we can build the browser with |mac_deployment_target| set to
  // |10.14| or above in |src/build/config/mac/mac_sdk.gni| it is not possible
  // detect the status of notification using the following code:

  // UNUserNotificationCenter *notificationCenter =
  //     [UNUserNotificationCenter currentNotificationCenter];
  // [notificationCenter getNotificationSettingsWithCompletionHandler:
  //     ^(UNNotificationSettings * _Nonnull settings) {
  //   switch (settings.authorizationStatus) {
  //     case UNAuthorizationStatusNotDetermined: {
  //       return false;
  //     }

  //     case UNAuthorizationStatusDenied: {
  //       return false;
  //     }

  //     case UNAuthorizationStatusAuthorized: {
  //       return true;
  //     }

  //     case UNAuthorizationStatusProvisional: {
  //       return true;
  //     }
  //   }
  // }];

  return true;
}

bool NotificationHelperMac::ShowMyFirstAdNotification() const {
  return false;
}

NotificationHelperMac* NotificationHelperMac::GetInstance() {
  return base::Singleton<NotificationHelperMac>::get();
}

NotificationHelper* NotificationHelper::GetInstance() {
  return NotificationHelperMac::GetInstance();
}

}  // namespace brave_ads
