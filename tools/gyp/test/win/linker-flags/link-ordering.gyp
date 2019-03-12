# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_ordering_exe',
      'type': 'executable',
      # These are so the names of the functions appear in the disassembly.
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3',
          'Optimization': '2',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'LinkIncremental': '1',
          'GenerateManifest': 'false',
          # Minimize the disassembly to just our code.
          'AdditionalOptions': [
            '/NODEFAULTLIB',
          ],
        },
      },
      'sources': [
        # Explicitly sorted the same way as the disassembly in the test .py.
        'main-crt.c',
        'z.cc',
        'x.cc',
        'y.cc',
        'hello.cc',
      ],
    },

    {
      'target_name': 'test_ordering_subdirs',
      'type': 'executable',
      # These are so the names of the functions appear in the disassembly.
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3',
          'Optimization': '2',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'LinkIncremental': '1',
          'GenerateManifest': 'false',
          # Minimize the disassembly to just our code.
          'AdditionalOptions': [
            '/NODEFAULTLIB',
          ],
        },
      },
      'sources': [
        # Explicitly sorted the same way as the disassembly in the test .py.
        'main-crt.c',
        'hello.cc',
        'b/y.cc',
        'a/z.cc',
      ],
    },


    {
      'target_name': 'test_ordering_subdirs_mixed',
      'type': 'executable',
      # These are so the names of the functions appear in the disassembly.
      'msvs_settings': {
        'VCCLCompilerTool': {
          'DebugInformationFormat': '3',
          'Optimization': '2',
        },
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true',
          'LinkIncremental': '1',
          'GenerateManifest': 'false',
          # Minimize the disassembly to just our code.
          'AdditionalOptions': [
            '/NODEFAULTLIB',
          ],
        },
      },
      'sources': [
        # Explicitly sorted the same way as the disassembly in the test .py.
        'main-crt.c',
        'a/x.cc',
        'hello.cc',
        'a/z.cc',
        'y.cc',
      ],
    },

  ]
}
