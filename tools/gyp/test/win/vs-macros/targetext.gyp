# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_targetext_executable',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(TargetDir)\\executable$(TargetExt)',
        },
      },
    },
    {
      'target_name': 'test_targetext_loadable_module',
      'type': 'loadable_module',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(TargetDir)\\loadable_module$(TargetExt)',
        },
      },
    },
    {
      'target_name': 'test_targetext_shared_library',
      'type': 'shared_library',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(TargetDir)\\shared_library$(TargetExt)',
        },
      },
    },
    {
      'target_name': 'test_targetext_static_library',
      'type': 'static_library',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLibrarianTool': {
          'OutputFile': '$(TargetDir)\\static_library$(TargetExt)',
        },
      },
    },
    {
      'target_name': 'test_targetext_product_extension',
      'type': 'executable',
      'sources': ['hello.cc'],
      'product_extension': 'library',
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(TargetDir)\\product_extension$(TargetExt)',
        },
      },
    },
  ]
}
