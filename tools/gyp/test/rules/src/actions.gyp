# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'pull_in_all_actions',
      'type': 'none',
      'dependencies': [
        'subdir1/executable.gyp:*',
        'subdir2/never_used.gyp:*',
        'subdir2/no_inputs.gyp:*',
        'subdir2/none.gyp:*',
        'subdir3/executable2.gyp:*',
        'subdir4/build-asm.gyp:*',
        'external/external.gyp:*',
      ],
    },
  ],
}
