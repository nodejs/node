# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    # Have to turn on function level linking here to get the function packaged
    # as a COMDAT so that it's eligible for optimizing away. Also turn on
    # debug information so that the symbol names for the code appear in the
    # dump (so we can verify if they are included in the final exe).
    {
      'target_name': 'test_optref_default',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'EnableFunctionLevelLinking': 'true',
          'DebugInformationFormat': '3',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
        },
      },
      'sources': ['opt-ref.cc'],
    },
    {
      'target_name': 'test_optref_no',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'EnableFunctionLevelLinking': 'true',
          'DebugInformationFormat': '3',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'OptimizeReferences': '1',
        },
      },
      'sources': ['opt-ref.cc'],
    },
    {
      'target_name': 'test_optref_yes',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'EnableFunctionLevelLinking': 'true',
          'DebugInformationFormat': '3',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'OptimizeReferences': '2',
        },
      },
      'sources': ['opt-ref.cc'],
    },
  ]
}
