# Copyright 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'default_configuration': 'A',
    'configurations': {
      'A': {
        'defines': ['SOMETHING'],
      },
      'B': {
        'inherit_from': ['A'],
      },
    },
    'cflags': ['-g'],
  },
  'targets': [
    {
      'target_name': 'configurations',
      'type': 'executable',
      'sources': [
        'configurations.c',
      ],
    },
  ],
}
