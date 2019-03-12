# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_analysis_on',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'EnablePREfast': 'true',
          'WarnAsError': 'true',
        },
      },
      'sources': ['uninit.cc'],
    },
    {
      'target_name': 'test_analysis_off',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'EnablePREfast': 'false',
          'WarnAsError': 'true',
        },
      },
      'sources': ['uninit.cc'],
    },
    {
      'target_name': 'test_analysis_unspec',
      'type': 'executable',
      'sources': ['uninit.cc'],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WarnAsError': 'true',
        },
      },
    },
  ]
}
