# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'program',
      'type': 'executable',
      'dependencies': [
        'lib1'
      ],
      'sources': [
        'program.cpp',
      ],
    },
    {
      'target_name': 'lib1',
      'type': 'static_library',
      'sources': [
        'lib1.hpp',
        'lib1.cpp',
      ],
    },
  ],
}
