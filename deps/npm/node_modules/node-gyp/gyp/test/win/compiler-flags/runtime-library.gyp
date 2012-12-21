# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_rl_md',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'RuntimeLibrary': '2'
        }
      },
      'sources': ['runtime-library-md.cc'],
    },
    {
      'target_name': 'test_rl_mdd',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'RuntimeLibrary': '3'
        }
      },
      'sources': ['runtime-library-mdd.cc'],
    },
    {
      'target_name': 'test_rl_mt',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'RuntimeLibrary': '0'
        }
      },
      'sources': ['runtime-library-mt.cc'],
    },
    {
      'target_name': 'test_rl_mtd',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'RuntimeLibrary': '1'
        }
      },
      'sources': ['runtime-library-mtd.cc'],
    },
  ]
}
