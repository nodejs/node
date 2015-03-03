# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'lib',
      'product_name': 'test_shared_lib',
      'type': 'shared_library',
      'sources': [
        'prog2.c',
        'func.c',
        'subdir1/func.c',
        'subdir2/func.c',
      ],
      'defines': [
        'PROG="prog2"',
      ],
      'conditions': [
        ['OS=="linux"', {
          'cflags': ['-fPIC'],
        }],
      ],
    },
  ],
}
