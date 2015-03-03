# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_nxcompat_default',
      'type': 'executable',
      'msvs_settings': {
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_nxcompat_no',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'DataExecutionPrevention': '1',
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_nxcompat_yes',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'DataExecutionPrevention': '2',
        },
      },
      'sources': ['hello.cc'],
    },
  ]
}
