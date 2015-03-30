# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'lib1',
      'type': 'shared_library',
      'sources': [
        'lib1.c',
      ],
    },
    {
      'target_name': 'lib2',
      'type': 'shared_library',
      'sources': [
        'lib2.c',
      ],
      'dependencies': [
        'lib1',
      ],
    },
  ],
  'conditions': [
    ['OS=="linux"', {
      'target_defaults': {
        # Support 64-bit shared libs (also works fine for 32-bit).
        'cflags': ['-fPIC'],
      },
    }],
  ],
}
