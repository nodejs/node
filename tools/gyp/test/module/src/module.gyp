# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'conditions': [
      ['OS=="win"', {
        'defines': ['PLATFORM_WIN'],
      }],
      ['OS=="mac"', {
        'defines': ['PLATFORM_MAC'],
      }],
      ['OS=="linux"', {
        'defines': ['PLATFORM_LINUX'],
        # Support 64-bit shared libs (also works fine for 32-bit).
        'cflags': ['-fPIC'],
        'ldflags': ['-ldl'],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'program',
      'type': 'executable',
      'dependencies': [
        'lib1',
        'lib2',
      ],
      'sources': [
        'program.c',
      ],
    },
    {
      'target_name': 'lib1',
      'type': 'loadable_module',
      'product_name': 'lib1',
      'product_prefix': '',
      'xcode_settings': {'OTHER_LDFLAGS': ['-dynamiclib'], 'MACH_O_TYPE': ''},
      'sources': [
        'lib1.c',
      ],
    },
    {
      'target_name': 'lib2',
      'product_name': 'lib2',
      'product_prefix': '',
      'type': 'loadable_module',
      'xcode_settings': {'OTHER_LDFLAGS': ['-dynamiclib'], 'MACH_O_TYPE': ''},
      'sources': [
        'lib2.c',
      ],
    },
  ],
}
