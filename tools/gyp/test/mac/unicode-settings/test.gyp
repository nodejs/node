# Copyright 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'myapp',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [ 'file.cc', ],
      'xcode_settings': {
        'BUNDLE_DISPLAY_NAME': 'α\011',
      },
      'postbuilds': [
        {
          'postbuild_name': 'envtest',
          'action': [ './test_bundle_display_name.sh', ],
        },
      ],
    },
  ],
}
