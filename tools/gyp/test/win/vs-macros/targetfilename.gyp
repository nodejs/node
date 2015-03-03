# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_targetfilename_executable',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(TargetDir)\\$(TargetFileName)',
        },
      },
    },
    {
      'target_name': 'test_targetfilename_loadable_module',
      'type': 'loadable_module',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(TargetDir)\\$(TargetFileName)',
        },
      },
    },
    {
      'target_name': 'test_targetfilename_shared_library',
      'type': 'loadable_module',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(TargetDir)\\$(TargetFileName)',
        },
      },
    },
    {
      'target_name': 'test_targetfilename_static_library',
      'type': 'static_library',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLibrarianTool': {
          'OutputFile': '$(TargetDir)\\$(TargetFileName)',
        },
      },
    },
    {
      'target_name': 'test_targetfilename_product_extension',
      'type': 'executable',
      'sources': ['hello.cc'],
      'product_extension': 'foo',
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(TargetDir)\\$(TargetFileName)',
        },
      },
    },
  ]
}
