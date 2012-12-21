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
        'subdir/prog2.gyp:prog2',
      ],
      'sources': [
        'prog1.c',
      ],
    },
  ],
}
