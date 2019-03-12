# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_additional_none',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
        }
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_additional_few',
      'type': 'executable',
      'msvs_settings': {
        'VCLinkerTool': {
          'AdditionalOptions': [
            '/dynamicbase:no',
          ]
        }
      },
      'sources': ['hello.cc'],
    },
  ]
}
