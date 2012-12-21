# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_opt_off',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'Optimization': '0'
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_opt_lev_size',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'Optimization': '1'
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_opt_lev_speed',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'Optimization': '2'
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_opt_lev_max',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'Optimization': '3'
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_opt_unset',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_opt_fpo',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'OmitFramePointers': 'true'
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_opt_fpo_off',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'OmitFramePointers': 'false'
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_opt_inline_off',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'InlineFunctionExpansion': '0'
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_opt_inline_manual',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'InlineFunctionExpansion': '1'
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_opt_inline_auto',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'InlineFunctionExpansion': '2'
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_opt_neither',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'FavorSizeOrSpeed': '0'
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_opt_speed',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'FavorSizeOrSpeed': '1'
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_opt_size',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'FavorSizeOrSpeed': '2'
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_opt_wpo',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WholeProgramOptimization': 'true'
        }
      },
      'sources': ['hello.cc'],
    },
  ]
}
