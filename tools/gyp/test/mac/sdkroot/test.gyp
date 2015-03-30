# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'mytarget',
      'type': 'executable',
      'sources': [ 'file.cc', ],
      'xcode_settings': {
        'SDKROOT': 'macosx%s',
      },
      'postbuilds': [
        {
          'postbuild_name': 'envtest',
          'action': [ './test_shorthand.sh', ],
        },
      ],
    },
    {
      'target_name': 'absolute',
      'type': 'executable',
      'sources': [ 'file.cc', ],
      'xcode_settings': {
        'SDKROOT': '<(sdk_path)',
      },
      'postbuilds': [
        {
          'postbuild_name': 'envtest',
          'action': [ './test_shorthand.sh', ],
        },
      ],
    },
  ],
}
