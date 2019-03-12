# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_expansions',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(OutDir)\\$(ProjectName)_plus_something.exe',
        },
      },
    },
    {
      'target_name': 'test_with_product_name',
      'product_name': 'prod_name',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(OutDir)\\$(ProjectName)_plus_something.exe',
        },
      },
    },
  ]
}
