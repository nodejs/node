# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'gypkext',
      'product_name': 'GypKext',
      'type': 'mac_kernel_extension',
      'sources': [
        'GypKext/GypKext.c',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'GypKext/GypKext-Info.plist',
      },
    },
  ],
}
