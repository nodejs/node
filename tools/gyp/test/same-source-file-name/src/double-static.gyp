# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'lib',
      'product_name': 'test_static_lib',
      'type': 'static_library',
      'sources': [
        'prog1.c',
        'func.c',
        'subdir1/func.c',
        'subdir2/func.c',
      ],
      'defines': [
        'PROG="prog1"',
      ],
    },
  ],
}
