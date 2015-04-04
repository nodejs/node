# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_targetname',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(TargetDir)\\$(TargetName)_plus_something1.exe',
        },
      },
    },
    {
      'target_name': 'test_targetname_with_prefix',
      'product_prefix': 'prod_prefix',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(TargetDir)\\$(TargetName)_plus_something2.exe',
        },
      },
    },
    {
      'target_name': 'test_targetname_with_prodname',
      'product_name': 'prod_name',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(TargetDir)\\$(TargetName)_plus_something3.exe',
        },
      },
    },
    {
      'target_name': 'test_targetname_with_prodname_with_prefix',
      'product_name': 'prod_name',
      'product_prefix': 'prod_prefix',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(TargetDir)\\$(TargetName)_plus_something4.exe',
        },
      },
    },
  ]
}
