# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'needs_chain',
      'type': 'executable',
      'sources': [
        'a.c',
        'main.c',
      ],
      'dependencies': ['chain'],
    },
    {
      'target_name': 'chain',
      'type': 'none',
      'dependencies': ['b/b.gyp:b'],
    },
    {
      'target_name': 'doesnt_need_chain',
      'type': 'executable',
      'sources': [
        'main.c',
      ],
      'dependencies': ['no_chain', 'other_chain'],
    },
    {
      'target_name': 'no_chain',
      'type': 'none',
      'sources': [
      ],
      'dependencies': ['b/b.gyp:b'],
      'dependencies_traverse': 0,
    },
    {
      'target_name': 'other_chain',
      'type': 'static_library',
      'sources': [
        'a.c',
      ],
      'dependencies': ['b/b.gyp:b3'],
    },
  ],
}
