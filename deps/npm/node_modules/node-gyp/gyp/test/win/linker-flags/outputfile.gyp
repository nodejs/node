# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_output_exe',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(OutDir)\\blorp.exe'
        },
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_output_exe2',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(OutDir)\\subdir\\blorp.exe'
        },
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_output_dll',
      'type': 'shared_library',
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(OutDir)\\blorp.dll'
        },
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_output_lib',
      'type': 'static_library',
      'msvs_settings': {
        'VCLibrarianTool': {
          'OutputFile': '$(OutDir)\\blorp.lib'
        },
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_output_lib2',
      'type': 'static_library',
      'msvs_settings': {
        'VCLibrarianTool': {
          'OutputFile': '$(OutDir)\\subdir\\blorp.lib'
        },
      },
      'sources': ['hello.cc'],
    },
  ]
}
