# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    # Turn on debug information so the incremental linking tables have a
    # visible symbolic name in the disassembly.
    {
      'target_name': 'test_incremental_unset',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
        },
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_incremental_default',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'LinkIncremental': '0',
        },
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_incremental_no',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'LinkIncremental': '1',
        },
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_incremental_yes',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'LinkIncremental': '2',
        }
      },
      'sources': ['hello.cc'],
    },
  ]
}
