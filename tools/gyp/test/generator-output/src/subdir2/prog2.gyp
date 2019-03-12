# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../symroot.gypi',
  ],
  'targets': [
    {
      'target_name': 'prog2',
      'type': 'executable',
      'include_dirs': [
        '..',
        '../inc1',
        'inc2',
        '../subdir3/inc3',
        'deeper',
      ],
      'dependencies': [
        '../subdir3/prog3.gyp:prog3',
      ],
      'sources': [
        'prog2.c',
      ],
    },
  ],
}
