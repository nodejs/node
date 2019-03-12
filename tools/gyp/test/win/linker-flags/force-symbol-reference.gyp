# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_force_reference_lib',
      'type': 'static_library',
      'sources': ['x.cc', 'y.cc'],
    },
    {
      'target_name': 'test_force_reference',
      'type': 'executable',
      # Turn on debug info to get symbols in disasm for the test code, and
      # turn on opt:ref to drop unused symbols to make sure we wouldn't
      # otherwise have the symbols.
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'AdditionalOptions': [
            '/OPT:REF',
          ],
          'ForceSymbolReferences': [
            '?x@@YAHXZ',
            '?y@@YAHXZ',
          ],
        },
      },
      'sources': ['hello.cc'],
      'dependencies': [
        'test_force_reference_lib',
      ],
    },
  ]
}
