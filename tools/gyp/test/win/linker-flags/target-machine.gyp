# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_target_link_x86',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'TargetMachine': '1',
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_target_link_x64',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'TargetMachine': '17',
        },
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_target_lib_x86',
      'type': 'static_library',
      'msvs_settings': {
        'VCLibrarianTool': {
          'TargetMachine': '1',
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_target_lib_x64',
      'type': 'static_library',
      'msvs_settings': {
        'VCLibrarianTool': {
          'TargetMachine': '17',
        },
      },
      'sources': ['hello.cc'],
    },
  ]
}
