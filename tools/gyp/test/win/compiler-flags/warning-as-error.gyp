# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_warn_as_error_false',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WarnAsError': 'false'
        }
      },
      'sources': ['warning-as-error.cc']
    },
    {
      'target_name': 'test_warn_as_error_true',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WarnAsError': 'true'
        }
      },
      'sources': ['warning-as-error.cc']
    },
    {
      'target_name': 'test_warn_as_error_unset',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
        }
      },
      'sources': ['warning-as-error.cc']
    },
  ]
}
