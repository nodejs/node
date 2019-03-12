# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'bare',
      'type': 'none',
      'actions': [
        {
          'action_name': 'action1',
          'inputs': [
            'bare.py',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/out.txt',
          ],
          'action': ['python', 'bare.py', '<(PRODUCT_DIR)/out.txt'],
          'msvs_cygwin_shell': 0,
        },
      ],
    },
  ],
}
