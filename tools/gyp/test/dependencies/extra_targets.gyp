# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'a',
      'type': 'static_library',
      'sources': [
        'a.c',
      ],
      # This only depends on the "d" target; other targets in c.gyp
      # should not become part of the build (unlike with 'c/c.gyp:*').
      'dependencies': ['c/c.gyp:d'],
    },
  ],
}
