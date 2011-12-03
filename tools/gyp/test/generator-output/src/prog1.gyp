# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'symroot.gypi',
  ],
  'targets': [
    {
      'target_name': 'prog1',
      'type': 'executable',
      'dependencies': [
        'subdir2/prog2.gyp:prog2',
      ],
      'include_dirs': [
        '.',
        'inc1',
        'subdir2/inc2',
        'subdir3/inc3',
        'subdir2/deeper',
      ],
      'sources': [
        'prog1.c',
      ],
    },
  ],
}
