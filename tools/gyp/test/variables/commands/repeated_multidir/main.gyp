# Copyright 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'main',
      'type': 'none',
      'dependencies': [
        'dir_1/test_1.gyp:target_1',
        'dir_2/test_2.gyp:target_2',
      ],
    },
  ],
}
