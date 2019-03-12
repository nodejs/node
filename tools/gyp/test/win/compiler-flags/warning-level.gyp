# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    # Level 1
    {
      'target_name': 'test_wl1_fail',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WarningLevel': '1',
          'WarnAsError': 'true',
        }
      },
      'sources': ['warning-level1.cc'],
    },
    {
      'target_name': 'test_wl1_pass',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WarningLevel': '1',
          'WarnAsError': 'true',
        }
      },
      'sources': ['warning-level2.cc'],
    },

    # Level 2
    {
      'target_name': 'test_wl2_fail',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WarningLevel': '2',
          'WarnAsError': 'true',
        }
      },
      'sources': ['warning-level2.cc'],
    },
    {
      'target_name': 'test_wl2_pass',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WarningLevel': '2',
          'WarnAsError': 'true',
        }
      },
      'sources': ['warning-level3.cc'],
    },

    # Level 3
    {
      'target_name': 'test_wl3_fail',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WarningLevel': '3',
          'WarnAsError': 'true',
        }
      },
      'sources': ['warning-level3.cc'],
    },
    {
      'target_name': 'test_wl3_pass',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WarningLevel': '3',
          'WarnAsError': 'true',
        }
      },
      'sources': ['warning-level4.cc'],
    },


    # Level 4
    {
      'target_name': 'test_wl4_fail',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WarningLevel': '4',
          'WarnAsError': 'true',
        }
      },
      'sources': ['warning-level4.cc'],
    },

    # Default level
    {
      'target_name': 'test_def_fail',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WarnAsError': 'true',
        }
      },
      'sources': ['warning-level1.cc'],
    },
    {
      'target_name': 'test_def_pass',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
        }
      },
      'sources': ['warning-level2.cc'],
    },

  ]
}
