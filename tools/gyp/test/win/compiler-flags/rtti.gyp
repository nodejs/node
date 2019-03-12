# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_rtti_off',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'RuntimeTypeInfo': 'false',
          'WarnAsError': 'true'
        }
      },
      'sources': ['rtti-on.cc'],
    },
    {
      'target_name': 'test_rtti_on',
      'type': 'executable',
      'msvs_settings': {
        'VCCLCompilerTool': {
          'RuntimeTypeInfo': 'true',
          'WarnAsError': 'true'
        }
      },
      'sources': ['rtti-on.cc'],
    },
    {
      'target_name': 'test_rtti_unset',
      'type': 'executable',
      'msvs_settings': {
      },
      'sources': ['rtti-on.cc'],
    },
  ]
}
