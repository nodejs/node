# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'prog1',
      'type': 'executable',
      'sources': [
        'prog1.c',
        'common.c',
      ],
    },
    {
      'target_name': 'prog2',
      'type': 'executable',
      'sources': [
        'prog2.c',
        'common.c',
      ],
    },
  ],
}
