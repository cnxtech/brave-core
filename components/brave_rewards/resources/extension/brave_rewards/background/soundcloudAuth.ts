// Copyright (c) 2019 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at http://mozilla.org/MPL/2.0/.

const soundcloudAuthHeaderNames = [
  'authorization',
  'x-csrf-token',
  'x-guest-token'
]

let soundcloudAuthHeaders = {}

chrome.runtime.onMessage.addListener((msg, sender, sendResponse) => {
  const action = typeof msg === 'string' ? msg : msg.type
  switch (action) {
    case 'getTwitterAPICredentials': {
      sendResponse(soundcloudAuthHeaders)
      break
    }
    default:
      break
  }
})

// Grab auth headers from soundcloud's normal requests
chrome.webRequest.onBeforeRequest.addListener(
  function ({ url }) {
    if(url) {
//      console.log(url)
    }
  },
   // Filters
  {
    urls: [
      'https://api-v2.soundcloud.com/*'
    ]
  })