# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'action',
      'type': 'none',
      'actions': [
        {
          'inputs': [ ],
          'outputs': [
            '<(PRODUCT_DIR)/result',
            '<(SHARED_INTERMEDIATE_DIR)/tempfile',
          ],
          'action_name': 'Test action',
          'action': ['./action.sh', '<(SHARED_INTERMEDIATE_DIR)/tempfile' ],
        },
        {
          'inputs': [
            '<(SHARED_INTERMEDIATE_DIR)/tempfile',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/other_result',
          ],
          'action_name': 'Other test action',
          'action': ['cp', '<(SHARED_INTERMEDIATE_DIR)/tempfile',
                           '<(PRODUCT_DIR)/other_result' ],
        },
      ],
    },
  ],
}

