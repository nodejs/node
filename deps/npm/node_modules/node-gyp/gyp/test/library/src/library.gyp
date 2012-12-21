# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'moveable_function%': 0,
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
      'type': '<(library)',
      'sources': [
        'lib1.c',
      ],
      'conditions': [
        ['moveable_function=="lib1"', {
          'sources': [
            'lib1_moveable.c',
          ],
        }],
      ],
    },
    {
      'target_name': 'lib2',
      'type': '<(library)',
      'sources': [
        'lib2.c',
      ],
      'conditions': [
        ['moveable_function=="lib2"', {
          'sources': [
            'lib2_moveable.c',
          ],
        }],
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
