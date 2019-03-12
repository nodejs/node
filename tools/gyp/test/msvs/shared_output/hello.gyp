# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': ['common.gypi'],
  'targets': [
    {
      'target_name': 'pull_in_there',
      'type': 'none',
      'dependencies': ['there/there.gyp:*'],
    },
    {
      'target_name': 'hello',
      'type': 'executable',
      'sources': [
        'hello.c',
      ],
    },
  ],
}
