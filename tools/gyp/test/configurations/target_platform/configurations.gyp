# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'configurations': {
      'Debug_Win32': {
        'msvs_configuration_platform': 'Win32',
      },
      'Debug_x64': {
        'msvs_configuration_platform': 'x64',
      },
    },
  },
  'targets': [
    {
      'target_name': 'left',
      'type': 'static_library',
      'sources': [
        'left.c',
      ],
      'configurations': {
        'Debug_Win32': {
          'msvs_target_platform': 'x64',
        },
      },
    },
    {
      'target_name': 'right',
      'type': 'static_library',
      'sources': [
        'right.c',
      ],
    },
    {
      'target_name': 'front_left',
      'type': 'executable',
      'dependencies': ['left'],
      'sources': [
        'front.c',
      ],
      'configurations': {
        'Debug_Win32': {
          'msvs_target_platform': 'x64',
        },
      },
    },
    {
      'target_name': 'front_right',
      'type': 'executable',
      'dependencies': ['right'],
      'sources': [
        'front.c',
      ],
    },
  ],
}
