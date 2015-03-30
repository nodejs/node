# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'lib1',
      'type': 'static_library',
      'sources': [ 'lib1.cc' ],
      'dependencies': [ 'lib_indirect' ],
    },
    {
      'target_name': 'lib2',
      'type': 'static_library',
      'sources': [ 'lib2.cc' ],
      'dependencies': [ 'lib_indirect' ],
    },
    {
      'target_name': 'lib3',
      'type': 'static_library',
      'sources': [ 'lib3.cc' ],
    },
    {
      'target_name': 'lib_indirect',
      'type': 'static_library',
      'sources': [ 'lib_indirect.cc' ],
    },
  ],
}
