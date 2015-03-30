# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'subdir_includes',
      'type': 'executable',
      'include_dirs': [
        '.',
        '../inc1',
        'inc2',
      ],
      'sources': [
        'subdir_includes.c',
      ],
    },
  ],
}
