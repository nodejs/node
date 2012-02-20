# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'create_intermediate',
      'type': 'none',
      'actions': [
        {
          'action_name': 'create_intermediate',
          'inputs': [
            'create_intermediate.py',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/intermediate',
            'ALWAYS.run.ALWAYS',
          ],
          'action': [
            'python', 'create_intermediate.py', '<(PRODUCT_DIR)/intermediate',
          ],
        },
      ],
    },
    {
      'target_name': 'dependent',
      'type': 'none',
      'dependencies': [
        'create_intermediate',
      ],
      'actions': [
        {
          'action_name': 'dependent',
          'inputs': [
            '<(PRODUCT_DIR)/intermediate',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/dependent'
          ],
          'action': [
            'touch', '<(PRODUCT_DIR)/dependent', '<(PRODUCT_DIR)/side_effect',
          ],
        },
      ],
    },
  ],
}
