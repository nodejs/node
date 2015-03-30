# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'f',
      'type': 'executable',
      'sources': [
        'f.c',
      ],
    },
    {
      'target_name': 'g',
      'type': 'executable',
      'sources': [
        'g.c',
      ],
      'dependencies': [
        'f',
      ],
    },
  ],
}
