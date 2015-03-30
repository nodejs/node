# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'dir2_target',
      'type': 'none',
      'dependencies': [
        '../dir1/dir1.gyp:dir1_target',
      ],
      'actions': [
        {
          'inputs': [ ],
          'outputs': [ '<(PRODUCT_DIR)/file.txt' ],
          'action_name': 'Test action',
          'action': ['cp', 'file.txt', '${BUILT_PRODUCTS_DIR}/file.txt' ],
        },
      ],
    },
  ],
}
