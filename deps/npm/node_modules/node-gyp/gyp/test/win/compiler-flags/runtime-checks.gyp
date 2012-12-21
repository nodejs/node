# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_brc_none',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'Optimization': '0',
        }
      },
      'sources': ['runtime-checks.cc'],
    },
    {
      'target_name': 'test_brc_1',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'Optimization': '0',
          'BasicRuntimeChecks': '3'
        }
      },
      'sources': ['runtime-checks.cc'],
    },
  ]
}
