# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'target_conditions': [
      ['has_lame==1', {
        'sources/': [
          ['exclude', 'lame'],
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'program',
      'type': 'executable',
      'variables': {
        'has_lame': 1,
      },
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'defines': [
        'FOO="^(_sources)"',
      ],
      'sources': [
        'program.cc',
        'this_is_lame.cc',
      ],
    },
  ],
}
