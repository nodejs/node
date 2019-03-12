# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'large_pdb_exe',
      'type': 'executable',
      'msvs_large_pdb': 1,
      'sources': [
        'main.cc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'ProgramDatabaseFile': '<(PRODUCT_DIR)/large_pdb_exe.exe.pdb',
        },
      },
    },
    {
      'target_name': 'small_pdb_exe',
      'type': 'executable',
      'msvs_large_pdb': 0,
      'sources': [
        'main.cc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'ProgramDatabaseFile': '<(PRODUCT_DIR)/small_pdb_exe.exe.pdb',
        },
      },
    },
    {
      'target_name': 'large_pdb_dll',
      'type': 'shared_library',
      'msvs_large_pdb': 1,
      'sources': [
        'dllmain.cc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'ProgramDatabaseFile': '<(PRODUCT_DIR)/large_pdb_dll.dll.pdb',
        },
      },
    },
    {
      'target_name': 'small_pdb_dll',
      'type': 'shared_library',
      'msvs_large_pdb': 0,
      'sources': [
        'dllmain.cc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'ProgramDatabaseFile': '<(PRODUCT_DIR)/small_pdb_dll.dll.pdb',
        },
      },
    },
    {
      'target_name': 'large_pdb_implicit_exe',
      'type': 'executable',
      'msvs_large_pdb': 1,
      'sources': [
        'main.cc',
      ],
      # No PDB file is specified. However, the msvs_large_pdb mechanism should
      # default to the appropriate <(PRODUCT_DIR)/<(TARGET_NAME).exe.pdb.
    },
    {
      'target_name': 'large_pdb_variable_exe',
      'type': 'executable',
      'msvs_large_pdb': 1,
      'sources': [
        'main.cc',
      ],
      # No PDB file is specified. However, the msvs_large_pdb_path variable
      # explicitly sets one.
      'variables': {
        'msvs_large_pdb_path': '<(PRODUCT_DIR)/foo.pdb',
      },
    },
    {
      'target_name': 'large_pdb_product_exe',
      'product_name': 'bar',
      'type': 'executable',
      'msvs_large_pdb': 1,
      'sources': [
        'main.cc',
      ],
      # No PDB file is specified. However, we've specified a product name so
      # it should use <(PRODUCT_DIR)/bar.exe.pdb.
    },
  ]
}
