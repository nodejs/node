# Copyright (c) 2010 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'subdirs',
      'type': 'none',
      'dependencies': [
        'subdir1/subdir1.gyp:*',
        'subdir2/subdir2.gyp:*',
      ],
    },
  ],
}
