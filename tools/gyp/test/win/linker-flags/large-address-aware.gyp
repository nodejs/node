# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_large_address_aware_no',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'LargeAddressAware': '1',
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_large_address_aware_yes',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'LargeAddressAware': '2',
        },
      },
      'sources': ['hello.cc'],
    },
  ]
}
