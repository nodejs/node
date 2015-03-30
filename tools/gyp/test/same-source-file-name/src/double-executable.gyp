# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'prog3',
      'type': 'executable',
      'sources': [
        'prog3.c',
        'func.c',
        'subdir1/func.c',
        'subdir2/func.c',
      ],
      'defines': [
        'PROG="prog3"',
      ],
    },
  ],
}
