# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../builddir.gypi',
  ],
  'targets': [
    {
      'target_name': 'prog3',
      'type': 'executable',
      'sources': [
        'prog3.c',
        '../../func3.c',
      ],
    },
  ],
}
