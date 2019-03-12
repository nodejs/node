# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    # Disable DYNAMICBASE for these tests because it implies/doesn't imply
    # FIXED in certain cases so it complicates the test for FIXED.
    {
      'target_name': 'test_fixed_default_exe',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'RandomizedBaseAddress': '1',
        },
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_fixed_default_dll',
      'type': 'shared_library',
      'msvs_settings': {
        'VCLinkerTool': {
          'RandomizedBaseAddress': '1',
        },
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_fixed_no',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'FixedBaseAddress': '1',
          'RandomizedBaseAddress': '1',
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_fixed_yes',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'FixedBaseAddress': '2',
          'RandomizedBaseAddress': '1',
        },
      },
      'sources': ['hello.cc'],
    },
  ]
}
