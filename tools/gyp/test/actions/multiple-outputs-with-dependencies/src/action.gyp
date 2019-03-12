# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'upper',
      'type': 'none',
      'actions': [{
        'action_name': 'upper_action',
        'inputs': ['<(PRODUCT_DIR)/out2.txt'],
        'outputs': ['<(PRODUCT_DIR)/result.txt'],
        'action': ['python', 'rcopy.py', '<@(_inputs)', '<@(_outputs)'],
      }],
    },
    {
      'target_name': 'lower',
      'type': 'none',
      'actions': [{
        'action_name': 'lower_action',
        'inputs': ['input.txt'],
        'outputs': ['<(PRODUCT_DIR)/out1.txt', '<(PRODUCT_DIR)/out2.txt'],
        'action': ['python', 'rcopy.py', '<@(_inputs)', '<@(_outputs)'],
      }],
    },
  ],
}
