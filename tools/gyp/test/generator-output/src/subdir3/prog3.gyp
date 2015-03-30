# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../symroot.gypi',
  ],
  'targets': [
    {
      'target_name': 'prog3',
      'type': 'executable',
      'include_dirs': [
        '..',
        '../inc1',
        '../subdir2/inc2',
        'inc3',
        '../subdir2/deeper',
      ],
      'sources': [
        'prog3.c',
      ],
    },
  ],
}
