# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_debug_off',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'GenerateDebugInformation': 'false'
        },
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_debug_on',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'GenerateDebugInformation': 'true'
        },
      },
      'sources': ['hello.cc'],
    },
  ]
}
