# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_deffile_multiple_fail',
      'type': 'shared_library',
      'sources': [
          'deffile.cc',
          'deffile.def',
          'deffile2.def',
      ],
    },
  ]
}
