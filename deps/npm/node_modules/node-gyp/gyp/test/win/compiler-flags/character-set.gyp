# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'test_cs_notset',
      'product_name': 'test_cs_notset',
      'type': 'executable',
      'msvs_configuration_attributes': {
        'CharacterSet': '0'
      },
      'sources': ['hello.cc'],
    },
    {
      'target_name': 'test_cs_unicode',
      'product_name': 'test_cs_unicode',
      'type': 'executable',
      'msvs_configuration_attributes': {
        'CharacterSet': '1'
      },
      'sources': ['character-set-unicode.cc'],
    },
    {
      'target_name': 'test_cs_mbcs',
      'product_name': 'test_cs_mbcs',
      'type': 'executable',
      'msvs_configuration_attributes': {
        'CharacterSet': '2'
      },
      'sources': ['character-set-mbcs.cc'],
    },
  ]
}
