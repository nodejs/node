# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'hello1',
      'product_name': 'alt1',
      'type': 'executable',
      'sources': [
        'hello.c',
      ],
    },
    {
      'target_name': 'hello2',
      'product_extension': 'stuff',
      'type': 'executable',
      'sources': [
        'hello.c',
      ],
    },
    {
      'target_name': 'hello3',
      'product_name': 'alt3',
      'product_extension': 'stuff',
      'product_prefix': 'yo',
      'type': 'executable',
      'sources': [
        'hello.c',
      ],
    },

    {
      'target_name': 'hello4',
      'product_name': 'alt4',
      'type': 'shared_library',
      'sources': [
        'hello.c',
      ],
    },
    {
      'target_name': 'hello5',
      'product_extension': 'stuff',
      'type': 'shared_library',
      'sources': [
        'hello.c',
      ],
    },
    {
      'target_name': 'hello6',
      'product_name': 'alt6',
      'product_extension': 'stuff',
      'product_prefix': 'yo',
      'type': 'shared_library',
      'sources': [
        'hello.c',
      ],
    },

    {
      'target_name': 'hello7',
      'product_name': 'alt7',
      'type': 'static_library',
      'sources': [
        'hello.c',
      ],
    },
    {
      'target_name': 'hello8',
      'product_extension': 'stuff',
      'type': 'static_library',
      'sources': [
        'hello.c',
      ],
    },
    {
      'target_name': 'hello9',
      'product_name': 'alt9',
      'product_extension': 'stuff',
      'product_prefix': 'yo',
      'type': 'static_library',
      'sources': [
        'hello.c',
      ],
    },
    {
      'target_name': 'hello10',
      'product_name': 'alt10',
      'product_extension': 'stuff',
      'product_prefix': 'yo',
      'product_dir': '<(PRODUCT_DIR)/bob',
      'type': 'executable',
      'sources': [
        'hello.c',
      ],
    },
    {
      'target_name': 'hello11',
      'product_name': 'alt11',
      'product_extension': 'stuff',
      'product_prefix': 'yo',
      'product_dir': '<(PRODUCT_DIR)/bob',
      'type': 'shared_library',
      'sources': [
        'hello.c',
      ],
    },
    {
      'target_name': 'hello12',
      'product_name': 'alt12',
      'product_extension': 'stuff',
      'product_prefix': 'yo',
      'product_dir': '<(PRODUCT_DIR)/bob',
      'type': 'static_library',
      'sources': [
        'hello.c',
      ],
    },
  ],
  'conditions': [
    ['OS=="linux"', {
      'target_defaults': {
        'cflags': ['-fPIC'],
      },
    }],
  ],
}
