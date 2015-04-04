# Copyright (c) 2010 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'exe',
      'type': 'executable',
      'sources': [
        'main.c',
      ],
      'dependencies': [
        'lib/shared.gyp:shared',
      ],
    },
  ],
}
