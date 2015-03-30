# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'exe',
      'type': 'executable',
      'dependencies': [
        'subdir2/subdir.gyp:foo',
      ],
    },
    {
      'target_name': 'exe2',
      'type': 'executable',
      'includes': [
        'test2.includes.gypi',
      ],
    },
  ],
  'includes': [
    'test2.toplevel_includes.gypi',
  ],
}
