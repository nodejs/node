# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This test both tests solibs and implicit_deps.
{
  'targets': [
    {
      'target_name': 'a',
      'type': 'shared_library',
      'sources': [ 'solib.cc' ],
    },
    {
      'target_name': 'b',
      'type': 'shared_library',
      'sources': [ 'solib.cc' ],
    },
    {
      'target_name': 'c',
      'type': 'executable',
      'sources': [ 'main.cc' ],
      'dependencies': [ 'a', 'b' ],
    },
  ],
  'conditions': [
    ['OS=="linux"', {
      'target_defaults': {
        'cflags': ['-fPIC'],
      },
    }],
  ],
}
