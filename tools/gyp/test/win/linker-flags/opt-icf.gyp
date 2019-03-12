# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    # Have to turn on function level linking here to get the function packaged
    # as a COMDAT so that it's eligible for merging. Also turn on debug
    # information so that the symbol names for the code appear in the dump.
    # Finally, specify non-incremental linking so that there's not a bunch of
    # extra "similar_function"s in the output (the ILT jump table).
    {
      'target_name': 'test_opticf_default',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'EnableFunctionLevelLinking': 'true',
          'DebugInformationFormat': '3',
          'Optimization': '0',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'LinkIncremental': '1',
        },
      },
      'sources': ['opt-icf.cc'],
    },
    {
      'target_name': 'test_opticf_no',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'EnableFunctionLevelLinking': 'true',
          'DebugInformationFormat': '3',
          'Optimization': '0',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'EnableCOMDATFolding': '1',
          'LinkIncremental': '1',
        },
      },
      'sources': ['opt-icf.cc'],
    },
    {
      'target_name': 'test_opticf_yes',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'EnableFunctionLevelLinking': 'true',
          'DebugInformationFormat': '3',
          'Optimization': '0',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'EnableCOMDATFolding': '2',
          'LinkIncremental': '1',
        },
      },
      'sources': ['opt-icf.cc'],
    },
  ]
}
