# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'allow_sharedlib_linksettings_propagation': 0,
  },
  'targets': [
    {
      'target_name': 'sharedlib',
      'type': 'shared_library',
      'sources': [ 'sharedlib.c' ],
      'link_settings': {
        'defines': [ 'TEST_DEFINE=1' ],
      },
      'conditions': [
        ['OS=="linux"', {
          # Support 64-bit shared libs (also works fine for 32-bit).
          'cflags': ['-fPIC'],
        }],
      ],
    },
    {
      'target_name': 'staticlib',
      'type': 'static_library',
      'sources': [ 'staticlib.c' ],
      'dependencies': [ 'sharedlib' ],
    },
    {
      'target_name': 'program',
      'type': 'executable',
      'sources': [ 'program.c' ],
      'dependencies': [ 'staticlib' ],
    },
  ],
}
