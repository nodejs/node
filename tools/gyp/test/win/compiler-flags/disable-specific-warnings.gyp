# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_disable_specific_warnings_set',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WarnAsError': 'true',
          'DisableSpecificWarnings': ['4700']
        }
      },
      'sources': ['disable-specific-warnings.cc']
    },
    {
      'target_name': 'test_disable_specific_warnings_unset',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WarnAsError': 'true'
        }
      },
      'sources': ['disable-specific-warnings.cc']
    },
  ]
}
