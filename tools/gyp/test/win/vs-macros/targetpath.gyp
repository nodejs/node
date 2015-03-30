# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_targetpath_executable',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(TargetPath)',
        },
      },
    },
    {
      'target_name': 'test_targetpath_loadable_module',
      'type': 'loadable_module',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(TargetPath)',
        },
      },
    },
    {
      'target_name': 'test_targetpath_shared_library',
      'type': 'loadable_module',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(TargetPath)',
        },
      },
    },
    {
      'target_name': 'test_targetpath_static_library',
      'type': 'static_library',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLibrarianTool': {
          'OutputFile': '$(TargetPath)',
        },
      },
    },
    {
      'target_name': 'test_targetpath_product_extension',
      'type': 'executable',
      'sources': ['hello.cc'],
      'product_extension': 'foo',
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(TargetPath)',
        },
      },
    },
  ]
}
