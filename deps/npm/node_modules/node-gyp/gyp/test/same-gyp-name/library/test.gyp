# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'duplicate_names',
      'type': 'shared_library',
      'dependencies': [
        'one/sub.gyp:one',
        'two/sub.gyp:two',
      ],
    },
  ],
}
