# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'configurations': {
      'Debug': {
        'msvs_configuration_platform': 'Win32',
      },
      'Debug_x64': {
        'inherit_from': ['Debug'],
        'msvs_configuration_platform': 'x64',
      },
    },
  },
  'targets': [
    {
      'target_name': 'configurations',
      'type': 'executable',
      'sources': [
        'configurations.c',
      ],
    },
    {
      'target_name': 'configurations64',
      'type': 'executable',
      'sources': [
        'configurations.c',
      ],
      'configurations': {
        'Debug': {
          'msvs_target_platform': 'x64',
        },
      },
    },
  ],
}
