# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'program1',
      'type': 'executable',
      'sources': [ 'program.cc' ],
      'dependencies': [ 'test2.gyp:lib1' ],
    },
    {
      'target_name': 'program2',
      'type': 'executable',
      'sources': [ 'program.cc' ],
      'dependencies': [ 'test2.gyp:lib2' ],
    },
    {
      'target_name': 'program3',
      'type': 'executable',
      'sources': [ 'program.cc' ],
      'dependencies': [ 'test2.gyp:lib3' ],
    },
  ],
}
