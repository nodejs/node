# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'test_app',
      'product_name': 'Test',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [ 'main.c', ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'Info.plist',
        'MY_VAR': 'some expansion',
        'OTHER_CFLAGS': [
          # Just some (more than one) random flags.
          '-fstack-protector-all',
          '-fno-strict-aliasing',
          '-DS="A Space"',  # Would normally be in 'defines'
        ],
      },
    },
  ],
}
