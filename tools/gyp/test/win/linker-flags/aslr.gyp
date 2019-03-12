# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_aslr_default',
      'type': 'executable',
      'msvs_settings': {
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_aslr_no',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'RandomizedBaseAddress': '1',
        },
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_aslr_yes',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'RandomizedBaseAddress': '2',
        }
      },
      'sources': ['hello.cc'],
    },
  ]
}
