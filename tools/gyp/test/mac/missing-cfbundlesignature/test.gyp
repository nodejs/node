# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'mytarget',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'Info.plist',
      },
    },
    {
      'target_name': 'myothertarget',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'Other-Info.plist',
      },
    },
    {
      'target_name': 'thirdtarget',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'Third-Info.plist',
      },
    },
  ],
}
