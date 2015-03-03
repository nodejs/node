# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'pull_in_all_actions',
      'type': 'none',
      'dependencies': [
        'subdir/input-rule-dirname.gyp:*',
      ],
    },
  ],
}
