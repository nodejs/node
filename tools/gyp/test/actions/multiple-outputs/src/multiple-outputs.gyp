# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'multiple-outputs',
      'type': 'none',
      'actions': [
        {
          'action_name': 'action1',
          'inputs': [],
          'outputs': [
            '<(PRODUCT_DIR)/out1.txt',
            '<(PRODUCT_DIR)/out2.txt',
          ],
          'action': ['python', 'touch.py', '<@(_outputs)'],
        },
      ],
    },
  ],
}
