# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_pdb_output_exe',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3'
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'ProgramDatabaseFile': 'output_exe.pdb',
        },
      },
    },
    {
      'target_name': 'test_pdb_output_dll',
      'type': 'shared_library',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3'
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'ProgramDatabaseFile': 'output_dll.pdb',
        },
      },
    },
    {
      'target_name': 'test_pdb_output_disabled',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '0'
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'false',
        },
      },
    },
  ]
}
