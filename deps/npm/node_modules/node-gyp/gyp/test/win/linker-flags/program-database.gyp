# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    # Verify that 'ProgramDataBase' option correctly makes it to LINK step in Ninja
    {
      # Verify that VC macros and windows paths work correctly
      'target_name': 'test_pdb_outdir',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3'
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'ProgramDatabaseFile': '$(OutDir)\\name_outdir.pdb',
        },
      },
    },
    {
      # Verify that GYP macros and POSIX paths work correctly
      'target_name': 'test_pdb_proddir',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3'
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'ProgramDatabaseFile': '<(PRODUCT_DIR)/name_proddir.pdb',
        },
      },
    },
  ]
}
