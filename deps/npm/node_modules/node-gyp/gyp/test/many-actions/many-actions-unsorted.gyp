# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'msvs_cygwin_dirs': ['../../../../<(DEPTH)/third_party/cygwin'],
  },
  'targets': [
    {
      'target_name': 'a',
      'type': 'none',
      'actions': [
        # Notice that the inputs go 0, 1, ..., 0, 1, .... This is to test
        # a regression in the msvs generator in _AddActions.
        {
          'action_name': 'do_0',
          'inputs': ['file0'],
          'outputs': ['<(PRODUCT_DIR)/generated_0.h'],
          'action': [
            'touch',
            '<(PRODUCT_DIR)/generated_0.h',
          ],
        },
        {
          'action_name': 'do_1',
          'inputs': ['file1'],
          'outputs': ['<(PRODUCT_DIR)/generated_1.h'],
          'action': [
            'touch',
            '<(PRODUCT_DIR)/generated_1.h',
          ],
        },
        {
          'action_name': 'do_2',
          'inputs': ['file2'],
          'outputs': ['<(PRODUCT_DIR)/generated_2.h'],
          'action': [
            'touch',
            '<(PRODUCT_DIR)/generated_2.h',
          ],
        },
        {
          'action_name': 'do_3',
          'inputs': ['file3'],
          'outputs': ['<(PRODUCT_DIR)/generated_3.h'],
          'action': [
            'touch',
            '<(PRODUCT_DIR)/generated_3.h',
          ],
        },
        {
          'action_name': 'do_4',
          'inputs': ['file4'],
          'outputs': ['<(PRODUCT_DIR)/generated_4.h'],
          'action': [
            'touch',
            '<(PRODUCT_DIR)/generated_4.h',
          ],
        },
        {
          'action_name': 'do_5',
          'inputs': ['file0'],
          'outputs': ['<(PRODUCT_DIR)/generated_5.h'],
          'action': [
            'touch',
            '<(PRODUCT_DIR)/generated_5.h',
          ],
        },
        {
          'action_name': 'do_6',
          'inputs': ['file1'],
          'outputs': ['<(PRODUCT_DIR)/generated_6.h'],
          'action': [
            'touch',
            '<(PRODUCT_DIR)/generated_6.h',
          ],
        },
        {
          'action_name': 'do_7',
          'inputs': ['file2'],
          'outputs': ['<(PRODUCT_DIR)/generated_7.h'],
          'action': [
            'touch',
            '<(PRODUCT_DIR)/generated_7.h',
          ],
        },
        {
          'action_name': 'do_8',
          'inputs': ['file3'],
          'outputs': ['<(PRODUCT_DIR)/generated_8.h'],
          'action': [
            'touch',
            '<(PRODUCT_DIR)/generated_8.h',
          ],
        },
        {
          'action_name': 'do_9',
          'inputs': ['file4'],
          'outputs': ['<(PRODUCT_DIR)/generated_9.h'],
          'action': [
            'touch',
            '<(PRODUCT_DIR)/generated_9.h',
          ],
        },
        {
          'action_name': 'do_10',
          'inputs': ['file0'],
          'outputs': ['<(PRODUCT_DIR)/generated_10.h'],
          'action': [
            'touch',
            '<(PRODUCT_DIR)/generated_10.h',
          ],
        },
        {
          'action_name': 'do_11',
          'inputs': ['file1'],
          'outputs': ['<(PRODUCT_DIR)/generated_11.h'],
          'action': [
            'touch',
            '<(PRODUCT_DIR)/generated_11.h',
          ],
        },
        {
          'action_name': 'do_12',
          'inputs': ['file2'],
          'outputs': ['<(PRODUCT_DIR)/generated_12.h'],
          'action': [
            'touch',
            '<(PRODUCT_DIR)/generated_12.h',
          ],
        },
        {
          'action_name': 'do_13',
          'inputs': ['file3'],
          'outputs': ['<(PRODUCT_DIR)/generated_13.h'],
          'action': [
            'touch',
            '<(PRODUCT_DIR)/generated_13.h',
          ],
        },
        {
          'action_name': 'do_14',
          'inputs': ['file4'],
          'outputs': ['<(PRODUCT_DIR)/generated_14.h'],
          'action': [
            'touch',
            '<(PRODUCT_DIR)/generated_14.h',
          ],
        },
      ],
    },
  ],
}
