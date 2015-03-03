# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_deffile_dll_ok',
      'type': 'shared_library',
      'sources': [
          'deffile.cc',
          'deffile.def',
      ],
    },
    {
      'target_name': 'test_deffile_dll_notexported',
      'type': 'shared_library',
      'sources': [
          'deffile.cc',
      ],
    },
    {
      'target_name': 'test_deffile_exe_ok',
      'type': 'executable',
      'sources': [
          'deffile.cc',
          'deffile.def',
      ],
    },
    {
      'target_name': 'test_deffile_exe_notexported',
      'type': 'executable',
      'sources': [
          'deffile.cc',
      ],
    },
  ]
}
