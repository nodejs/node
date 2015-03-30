# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'file-in',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [ 'file.in' ],
        },
      ],
    },
    {
      'target_name': 'file-out',
      'type': 'none',
      'dependencies': [ 'file-in' ],
      'actions': [
        {
          'action_name': 'copy-file',
          'inputs': [ '$(strip <(PRODUCT_DIR)/file.in)' ],
          'outputs': [ '<(PRODUCT_DIR)/file.out' ],
          'action': [ 'cp', '$(strip <(PRODUCT_DIR)/file.in)', '<(PRODUCT_DIR)/file.out' ],
        }
      ],
    },
  ],
}
