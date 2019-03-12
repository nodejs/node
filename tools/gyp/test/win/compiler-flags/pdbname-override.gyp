# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_pdbname',
      'type': 'executable',
      'sources': [
        'hello.cc',
        'pdbname.cc',
      ],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3',
          'ProgramDataBaseFileName': '<(PRODUCT_DIR)/compiler_generated.pdb',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'ProgramDatabaseFile': '<(PRODUCT_DIR)/linker_generated.pdb',
        },
      },
    },
  ]
}
