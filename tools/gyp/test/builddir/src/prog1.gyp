# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'builddir.gypi',
  ],
  'targets': [
    {
      'target_name': 'pull_in_all',
      'type': 'none',
      'dependencies': [
        'prog1',
        'subdir2/prog2.gyp:prog2',
        'subdir2/subdir3/prog3.gyp:prog3',
        'subdir2/subdir3/subdir4/prog4.gyp:prog4',
        'subdir2/subdir3/subdir4/subdir5/prog5.gyp:prog5',
      ],
    },
    {
      'target_name': 'prog1',
      'type': 'executable',
      'sources': [
        'prog1.c',
        'func1.c',
      ],
    },
  ],
}
