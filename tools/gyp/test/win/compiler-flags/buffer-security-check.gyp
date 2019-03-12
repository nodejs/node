# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    # Turn debug information on so that we can see the name of the buffer
    # security check cookie in the disassembly.
    {
      'target_name': 'test_bsc_unset',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
        },
      },
      'sources': ['buffer-security.cc'],
    },
    {
      'target_name': 'test_bsc_off',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'BufferSecurityCheck': 'false',
          'DebugInformationFormat': '3',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
        },
      },
      'sources': ['buffer-security.cc'],
    },
    {
      'target_name': 'test_bsc_on',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'BufferSecurityCheck': 'true',
          'DebugInformationFormat': '3',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
        },
      },
      'sources': ['buffer-security.cc'],
    },
  ]
}
