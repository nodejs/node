# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'prog1',
      'type': 'executable',
      'defines': [
        'PROG="prog1"',
      ],
      'sources': [
        'prog1.c',
        'func.c',
        # Uncomment to test same-named files in different directories,
        # which Visual Studio doesn't support.
        #'subdir1/func.c',
        #'subdir2/func.c',
      ],
    },
    {
      'target_name': 'prog2',
      'type': 'executable',
      'defines': [
        'PROG="prog2"',
      ],
      'sources': [
        'prog2.c',
        'func.c',
        # Uncomment to test same-named files in different directories,
        # which Visual Studio doesn't support.
        #'subdir1/func.c',
        #'subdir2/func.c',
      ],
    },
  ],
}
