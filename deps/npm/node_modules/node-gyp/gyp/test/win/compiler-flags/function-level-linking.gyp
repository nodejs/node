# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_fll_off',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'EnableFunctionLevelLinking': 'false'
        }
      },
      'sources': ['function-level-linking.cc'],
    },
    {
      'target_name': 'test_fll_on',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'EnableFunctionLevelLinking': 'true',
        }
      },
      'sources': ['function-level-linking.cc'],
    },
  ]
}
