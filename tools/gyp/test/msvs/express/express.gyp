# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'express',
      'type': 'executable',
      'dependencies': [
        'base/base.gyp:a',
        'base/base.gyp:b',
      ],
      'sources': [
        'main.c',
      ],
    },
  ],
}
