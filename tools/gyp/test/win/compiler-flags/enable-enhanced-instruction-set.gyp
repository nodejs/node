# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'sse_extensions',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'EnableEnhancedInstructionSet': '1',  # StreamingSIMDExtensions
        }
      },
      'sources': ['enable-enhanced-instruction-set.cc'],
    },
    {
      'target_name': 'sse2_extensions',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'EnableEnhancedInstructionSet': '2',  # StreamingSIMDExtensions2
        }
      },
      'sources': ['enable-enhanced-instruction-set.cc'],
    },
  ],
  'conditions': [
    ['MSVS_VERSION[0:4]>"2010"', {
      'targets': [
        {
          'target_name': 'avx_extensions',
          'type': 'executable',
          'msvs_settings': {
            'VCCLCompilerTool': {
              'EnableEnhancedInstructionSet': '3',  # AdvancedVectorExtensions
            }
          },
          'sources': ['enable-enhanced-instruction-set.cc'],
        },
        {
          'target_name': 'no_extensions',
          'type': 'executable',
          'msvs_settings': {
            'VCCLCompilerTool': {
              'EnableEnhancedInstructionSet': '4',  # NoExtensions
            }
          },
          'sources': ['enable-enhanced-instruction-set.cc'],
        },
      ],
    }],
    ['MSVS_VERSION[0:4]>="2013"', {
      'targets': [
        {
          'target_name': 'avx2_extensions',
          'type': 'executable',
          'msvs_settings': {
            'VCCLCompilerTool': {
              'EnableEnhancedInstructionSet': '5',  # AdvancedVectorExtensions2
            }
          },
          'sources': ['enable-enhanced-instruction-set.cc'],
        },
      ],
    }],
  ],
}
