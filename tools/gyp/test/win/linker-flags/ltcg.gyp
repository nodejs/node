# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'test_ltcg_off',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WholeProgramOptimization': 'false',
        },
        'VCLinkerTool': {
          'LinkTimeCodeGeneration': '0',
        },
      },
      'sources': [
        'inline_test.h',
        'inline_test.cc',
        'inline_test_main.cc',
      ],
    },
    {
      'target_name': 'test_ltcg_on',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WholeProgramOptimization': 'true',  # /GL
        },
        'VCLinkerTool': {
          'LinkTimeCodeGeneration': '1',       # /LTCG
        },
      },
      'sources': [
        'inline_test.h',
        'inline_test.cc',
        'inline_test_main.cc',
      ],
    },
  ]
}
