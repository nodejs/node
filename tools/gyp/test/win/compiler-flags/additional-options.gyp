# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_additional_none',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WarningLevel': '4',
          'WarnAsError': 'true',
        }
      },
      'sources': ['additional-options.cc'],
    },
    {
      'target_name': 'test_additional_one',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WarningLevel': '4',
          'WarnAsError': 'true',
          'AdditionalOptions': [ '/W1' ],
        }
      },
      'sources': ['additional-options.cc'],
    },
  ]
}
