# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'all',
      'type': 'none',
      'dependencies': [
        'a',
        'b',
      ],
    },
    {
      'target_name': 'a',
      'type': 'executable',
      'sources': [
        'a.c',
      ],
      'dependencies': [
        'c',
        'd',
      ],
    },
    {
      'target_name': 'b',
      'type': 'executable',
      'sources': [
        'b.c',
      ],
      'dependencies': [
        'd',
        'e',
      ],
    },
    {
      'target_name': 'c',
      'type': 'executable',
      'sources': [
        'c.c',
      ],
    },
    {
      'target_name': 'd',
      'type': 'none',
      'sources': [
        'd.c',
      ],
      'dependencies': [
        'f',
        'g',
      ],
    },
    {
      'target_name': 'e',
      'type': 'executable',
      'sources': [
        'e.c',
      ],
    },
    {
      'target_name': 'f',
      'type': 'static_library',
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
    },
  ],
}
