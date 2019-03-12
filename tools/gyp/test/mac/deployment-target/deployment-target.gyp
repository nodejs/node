# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'macosx-version-min-10.5',
      'type': 'executable',
      'sources': [ 'check-version-min.c', ],
      'defines': [ 'GYPTEST_MAC_VERSION_MIN=1050', ],
      'xcode_settings': {
        'SDKROOT': 'macosx',
        'MACOSX_DEPLOYMENT_TARGET': '10.5',
      },
    },
    {
      'target_name': 'macosx-version-min-10.6',
      'type': 'executable',
      'sources': [ 'check-version-min.c', ],
      'defines': [ 'GYPTEST_MAC_VERSION_MIN=1060', ],
      'xcode_settings': {
        'SDKROOT': 'macosx',
        'MACOSX_DEPLOYMENT_TARGET': '10.6',
      },
    },
  ],
}

