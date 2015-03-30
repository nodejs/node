# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'test_app',
      'product_name': 'Test App Gyp',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [
        'main.c',
      ],
      'mac_bundle_resources': [
        'resource.sb',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'Info.plist',
        'ORDER_FILE': 'app.order',
        'GCC_PREFIX_HEADER': 'header.h',
        'GCC_PRECOMPILE_PREFIX_HEADER': 'YES',
      },
    },
  ],
}
