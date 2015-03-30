# Copyright 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_base_specified_exe',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'BaseAddress': '0x00420000',
        },
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_base_specified_dll',
      'type': 'shared_library',
      'msvs_settings': {
        'VCLinkerTool': {
          'BaseAddress': '0x10420000',
        },
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_base_default_exe',
      'type': 'executable',
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_base_default_dll',
      'type': 'shared_library',
      'sources': ['hello.cc'],
    },
  ]
}
