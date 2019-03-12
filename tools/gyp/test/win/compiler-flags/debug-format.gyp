# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test-debug-format-off',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '0'
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test-debug-format-oldstyle',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '1'
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test-debug-format-pdb',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3'
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test-debug-format-editcontinue',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '4'
        }
      },
      'sources': ['hello.cc'],
    },
  ]
}
