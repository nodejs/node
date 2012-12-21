# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    # Verify that 'Profile' option correctly makes it to LINK steup in Ninja
    {
      'target_name': 'test_profile_true',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3'
        },
        'VCLinkerTool': {
          'Profile': 'true',
          'GenerateDebugInformation': 'true',
        },
      },
    },
    {
      'target_name': 'test_profile_false',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3'
        },
        'VCLinkerTool': {
          'Profile': 'false',
          'GenerateDebugInformation': 'true',
        },
      },
    },
    {
      'target_name': 'test_profile_default',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3'
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
        },
      },
    },
  ]
}
