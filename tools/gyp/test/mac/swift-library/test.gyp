# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'SwiftFramework',
      'product_name': 'SwiftFramework',
      'type': 'shared_library',
      'mac_bundle': 1,
      'xcode_settings': {
        'INFOPLIST_FILE': 'Info.plist',
        'CODE_SIGNING_REQUIRED': 'NO',
        'CONFIGURATION_BUILD_DIR':'build/Default',
      },
      'sources': [
        'file.swift',
      ],
    },
  ],
}
